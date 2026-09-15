#include "FastGameCharacterComponent.h"
#include "FastGameSubsystem.h"
#include "FastGameClient.h"
#include "FastGameBlueprintConvert.h"
#include "FastGameHttp.h"
#include "FastGameLatentActions.h"
#include "FastGameGameplayDirectorComponent.h"
#include "FastGameCharacterControllerComponent.h"
#include "FastGameCameraControllerComponent.h"
#include "FastGameAbilityRuntimeComponent.h"
#include "Async/Async.h"
#include "Engine/GameInstance.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace FastGameCharacterComponentUtil
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

void UFastGameCharacterComponent::FetchCharacter(
	const FString& GameCode,
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
		Message = TEXT("FastGame: no world for Fetch Character");
		OnCharacterFetched.Broadcast(false, TEXT(""), Message);
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
	UFastGameSubsystem* Subsystem = FastGameCharacterComponentUtil::ResolveSubsystem(this, Err);
	if (!Subsystem)
	{
		State->bSuccess = false;
		State->StatusCode = 0;
		State->Message = Err;
		State->bFinished = true;
		OnCharacterFetched.Broadcast(false, TEXT(""), Err);
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
		OnCharacterFetched.Broadcast(false, TEXT(""), LocalErr);
		return;
	}

	const FString ResolvedGame = FastGameCharacterComponentUtil::ResolveGameCode(Subsystem, GameCode);
	if (CharacterId.IsNone())
	{
		const FString LocalErr = TEXT("FastGame: CharacterId is not set");
		State->bSuccess = false;
		State->StatusCode = 0;
		State->Message = LocalErr;
		State->bFinished = true;
		OnCharacterFetched.Broadcast(false, TEXT(""), LocalErr);
		return;
	}
	if (ResolvedGame.IsEmpty())
	{
		const FString LocalErr = TEXT("FastGame: GameCode is empty — call Initialize Game first");
		State->bSuccess = false;
		State->StatusCode = 0;
		State->Message = LocalErr;
		State->bFinished = true;
		OnCharacterFetched.Broadcast(false, TEXT(""), LocalErr);
		return;
	}

	TWeakObjectPtr<UFastGameCharacterComponent> WeakThis(this);
	Client->Content->GetCharacter(ResolvedGame, CharacterId.ToString(),
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
				if (UFastGameCharacterComponent* Self = WeakThis.Get())
				{
					if (bOk)
					{
						Self->ApplyEntityTipJson(Body);
					}
					Self->OnCharacterFetched.Broadcast(bOk, Body, Msg);
				}
			});
		});
}

void UFastGameCharacterComponent::ApplyEntityTipJson(const FString& JsonBody)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonBody);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return;
	}
	const TSharedPtr<FJsonObject>* PayloadPtr = nullptr;
	TSharedPtr<FJsonObject> Payload = Root;
	if (Root->TryGetObjectField(TEXT("payload"), PayloadPtr) && PayloadPtr && PayloadPtr->IsValid())
	{
		Payload = *PayloadPtr;
	}
	FString Kind;
	if (Payload->TryGetStringField(TEXT("kind"), Kind) && !Kind.IsEmpty())
	{
		EntityKind = FName(*Kind);
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	UFastGameGameplayDirectorComponent* Director = Owner->FindComponentByClass<UFastGameGameplayDirectorComponent>();
	UFastGameCharacterControllerComponent* Character =
		Owner->FindComponentByClass<UFastGameCharacterControllerComponent>();
	UFastGameCameraControllerComponent* Camera =
		Director && Director->CameraController
			? Director->CameraController.Get()
			: Owner->FindComponentByClass<UFastGameCameraControllerComponent>();
	UFastGameAbilityRuntimeComponent* Abilities =
		Owner->FindComponentByClass<UFastGameAbilityRuntimeComponent>();

	if (Character)
	{
		Character->ApplyLocomotionFromTipJson(JsonBody);
	}
	if (Camera)
	{
		Camera->ApplyAllowedCamerasFromTipJson(JsonBody);
	}
	if (Abilities)
	{
		Abilities->LoadAbilitiesFromTipJson(JsonBody);
	}

	FString Move;
	FString Cam;
	Payload->TryGetStringField(TEXT("movement_profile"), Move);
	Payload->TryGetStringField(TEXT("camera_profile"), Cam);
	if (Director)
	{
		if (!Move.IsEmpty())
		{
			Director->ApplyMovementProfile(FName(*Move));
		}
		if (!Cam.IsEmpty())
		{
			Director->ApplyCameraProfile(FName(*Cam));
		}
	}
	else
	{
		if (Character && !Move.IsEmpty())
		{
			Character->ApplyMovementProfile(FName(*Move));
		}
		if (Camera && !Cam.IsEmpty())
		{
			Camera->ApplyCameraProfile(FName(*Cam));
		}
	}
}
