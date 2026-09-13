#include "FastGameLootRuntimeComponent.h"
#include "FastGameMapComponent.h"
#include "FastGameParamRuntimeComponent.h"
#include "FastGameSubsystem.h"
#include "FastGameClient.h"
#include "FastGameBlueprintConvert.h"
#include "FastGameHttp.h"
#include "FastGameLatentActions.h"
#include "Async/Async.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"

namespace FastGameLootRuntimeUtil
{
	static UFastGameSubsystem* ResolveSubsystem(const UActorComponent* Component, FString& OutError)
	{
		OutError.Reset();
		if (!Component)
		{
			OutError = TEXT("FastGame: invalid component");
			return nullptr;
		}
		const UWorld* World = Component->GetWorld();
		if (!World)
		{
			OutError = TEXT("FastGame: no world");
			return nullptr;
		}
		UGameInstance* GI = World->GetGameInstance();
		if (!GI)
		{
			OutError = TEXT("FastGame: no game instance");
			return nullptr;
		}
		return GI->GetSubsystem<UFastGameSubsystem>();
	}

	static FString ResolveGameCode(UFastGameSubsystem* Subsystem, const FString& GameCode)
	{
		const FString Trimmed = GameCode.TrimStartAndEnd();
		if (!Trimmed.IsEmpty())
		{
			return Trimmed;
		}
		return Subsystem ? Subsystem->GetGameCode().TrimStartAndEnd() : FString();
	}
}

void UFastGameLootRuntimeComponent::BeginPlay()
{
	Super::BeginPlay();
	ResolveModules();
}

void UFastGameLootRuntimeComponent::ResolveModules()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	if (!Map)
	{
		Map = Owner->FindComponentByClass<UFastGameMapComponent>();
	}
	if (!Params)
	{
		Params = Owner->FindComponentByClass<UFastGameParamRuntimeComponent>();
	}
}

void UFastGameLootRuntimeComponent::GetLootTable(
	const FString& GameCode,
	FName LootTableId,
	FLatentActionInfo LatentInfo,
	bool& bSuccess,
	int32& StatusCode,
	FString& Message,
	FString& JsonBody)
{
	JsonBody.Reset();
	bSuccess = false;
	StatusCode = 0;
	Message.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		Message = TEXT("FastGame: no world for Get Loot Table");
		OnLootFailed.Broadcast(Message);
		return;
	}

	FLatentActionManager& LatentManager = World->GetLatentActionManager();
	if (LatentManager.FindExistingAction<FFastGameRequestLatentAction>(LatentInfo.CallbackTarget, LatentInfo.UUID))
	{
		return;
	}

	const TSharedRef<FFastGameRequestLatentState> State = MakeShared<FFastGameRequestLatentState>();
	FFastGameRequestLatentAction* Action = new FFastGameRequestLatentAction(LatentInfo, State);
	Action->bSuccessOut = &bSuccess;
	Action->StatusCodeOut = &StatusCode;
	Action->MessageOut = &Message;
	Action->JsonBodyOut = &JsonBody;
	LatentManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, Action);

	FString Err;
	UFastGameSubsystem* Subsystem = FastGameLootRuntimeUtil::ResolveSubsystem(this, Err);
	if (!Subsystem)
	{
		State->bSuccess = false;
		State->StatusCode = 0;
		State->Message = Err;
		State->bFinished = true;
		OnLootTableFetched.Broadcast(false, TEXT(""), Err);
		OnLootFailed.Broadcast(Err);
		return;
	}

	TSharedPtr<FFastGameClient> Client = Subsystem->GetClient();
	if (!Client.IsValid())
	{
		const FString LocalErr = TEXT("FastGame: client not initialized");
		State->bSuccess = false;
		State->StatusCode = 0;
		State->Message = LocalErr;
		State->bFinished = true;
		OnLootTableFetched.Broadcast(false, TEXT(""), LocalErr);
		OnLootFailed.Broadcast(LocalErr);
		return;
	}

	const FString ResolvedGame = FastGameLootRuntimeUtil::ResolveGameCode(Subsystem, GameCode);
	TWeakObjectPtr<UFastGameLootRuntimeComponent> WeakThis(this);
	Client->Content->GetLootTable(ResolvedGame, LootTableId.ToString(),
		[WeakThis, State](bool bOk, TSharedPtr<FJsonObject> Json, FString Error)
		{
			const FString Body = FastGameBlueprintConvert::JsonObjectToString(Json);
			int32 Code = 0;
			FString Msg;
			FFastGameHttp::ParseStatusFromError(bOk, Error, Code, Msg);
			AsyncTask(ENamedThreads::GameThread, [WeakThis, State, bOk, Body, Code, Msg]()
			{
				State->JsonBody = Body;
				State->bSuccess = bOk;
				State->StatusCode = Code;
				State->Message = Msg;
				State->bFinished = true;
				if (UFastGameLootRuntimeComponent* Self = WeakThis.Get())
				{
					Self->OnLootTableFetched.Broadcast(bOk, Body, Msg);
					if (!bOk)
					{
						Self->OnLootFailed.Broadcast(Msg);
					}
				}
			});
		});
}

void UFastGameLootRuntimeComponent::OpenLoot(
	const FString& GameCode,
	FName PickupId,
	FName PlacementId,
	FName LootTableId,
	FName ModeId,
	FLatentActionInfo LatentInfo,
	bool& bSuccess,
	int32& StatusCode,
	FString& Message,
	FString& JsonBody)
{
	JsonBody.Reset();
	bSuccess = false;
	StatusCode = 0;
	Message.Reset();
	ResolveModules();

	UWorld* World = GetWorld();
	if (!World)
	{
		Message = TEXT("FastGame: no world for Open Loot");
		OnLootFailed.Broadcast(Message);
		return;
	}

	FLatentActionManager& LatentManager = World->GetLatentActionManager();
	if (LatentManager.FindExistingAction<FFastGameRequestLatentAction>(LatentInfo.CallbackTarget, LatentInfo.UUID))
	{
		return;
	}

	const TSharedRef<FFastGameRequestLatentState> State = MakeShared<FFastGameRequestLatentState>();
	FFastGameRequestLatentAction* Action = new FFastGameRequestLatentAction(LatentInfo, State);
	Action->bSuccessOut = &bSuccess;
	Action->StatusCodeOut = &StatusCode;
	Action->MessageOut = &Message;
	Action->JsonBodyOut = &JsonBody;
	LatentManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, Action);

	FString Err;
	UFastGameSubsystem* Subsystem = FastGameLootRuntimeUtil::ResolveSubsystem(this, Err);
	if (!Subsystem)
	{
		State->bSuccess = false;
		State->StatusCode = 0;
		State->Message = Err;
		State->bFinished = true;
		OnLootOpened.Broadcast(false, TEXT(""), Err);
		OnLootFailed.Broadcast(Err);
		return;
	}

	TSharedPtr<FFastGameClient> Client = Subsystem->GetClient();
	if (!Client.IsValid())
	{
		const FString LocalErr = TEXT("FastGame: client not initialized");
		State->bSuccess = false;
		State->StatusCode = 0;
		State->Message = LocalErr;
		State->bFinished = true;
		OnLootOpened.Broadcast(false, TEXT(""), LocalErr);
		OnLootFailed.Broadcast(LocalErr);
		return;
	}

	const FString ResolvedGame = FastGameLootRuntimeUtil::ResolveGameCode(Subsystem, GameCode);
	const FString MapId = Map ? Map->MapId.ToString() : FString();
	TWeakObjectPtr<UFastGameLootRuntimeComponent> WeakThis(this);
	Client->Content->OpenLoot(
		ResolvedGame,
		MapId,
		ModeId.ToString(),
		PickupId.ToString(),
		PlacementId.ToString(),
		LootTableId.ToString(),
		[WeakThis, State](bool bOk, TSharedPtr<FJsonObject> Json, FString Error)
		{
			const FString Body = FastGameBlueprintConvert::JsonObjectToString(Json);
			int32 Code = 0;
			FString Msg;
			FFastGameHttp::ParseStatusFromError(bOk, Error, Code, Msg);
			AsyncTask(ENamedThreads::GameThread, [WeakThis, State, bOk, Body, Code, Msg]()
			{
				State->JsonBody = Body;
				State->bSuccess = bOk;
				State->StatusCode = Code;
				State->Message = Msg;
				State->bFinished = true;
				if (UFastGameLootRuntimeComponent* Self = WeakThis.Get())
				{
					if (bOk && Self->Params && !Self->OpenParamName.IsNone())
					{
						Self->Params->SetAnimatorBool(Self->OpenParamName, true);
					}
					Self->OnLootOpened.Broadcast(bOk, Body, Msg);
					if (!bOk)
					{
						Self->OnLootFailed.Broadcast(Msg);
					}
				}
			});
		});
}
