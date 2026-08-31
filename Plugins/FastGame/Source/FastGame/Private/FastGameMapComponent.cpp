#include "FastGameMapComponent.h"
#include "FastGameSubsystem.h"
#include "FastGameClient.h"
#include "FastGameBlueprintConvert.h"
#include "FastGameHttp.h"
#include "FastGameLatentActions.h"
#include "Async/Async.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

namespace FastGameMapComponentUtil
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

	static FString ResolveModeId(FName ComponentModeId)
	{
		return ComponentModeId.IsNone() ? FString() : ComponentModeId.ToString();
	}

	static bool IsOnlineMode(const FString& ModeId)
	{
		const FString Trimmed = ModeId.TrimStartAndEnd();
		return !Trimmed.IsEmpty() && !Trimmed.Equals(TEXT("solo"), ESearchCase::IgnoreCase);
	}

	static FString ExtractEngineSceneFromMapConfig(const FString& JsonBody)
	{
		const TSharedPtr<FJsonObject> Root = FastGameBlueprintConvert::ParseJsonObject(JsonBody);
		if (!Root.IsValid())
		{
			return FString();
		}

		const TSharedPtr<FJsonObject>* PayloadObj = nullptr;
		if (Root->TryGetObjectField(TEXT("payload"), PayloadObj) && PayloadObj && PayloadObj->IsValid())
		{
			FString EngineScene;
			if ((*PayloadObj)->TryGetStringField(TEXT("engine_scene"), EngineScene))
			{
				return EngineScene;
			}
		}

		FString EngineScene;
		Root->TryGetStringField(TEXT("engine_scene"), EngineScene);
		return EngineScene;
	}

	static void FinishTravelMap(
		const TSharedRef<FFastGameRequestLatentState>& State,
		EFastGameTravelMapPin Pin,
		bool bOk,
		int32 Code,
		const FString& Msg,
		const FFastGameBPSeatMint& Seat = FFastGameBPSeatMint())
	{
		State->TravelMapPin = Pin;
		State->SeatMint = Seat;
		State->bSuccess = bOk;
		State->StatusCode = Code;
		State->Message = Msg;
		State->bFinished = true;
	}
}

void UFastGameMapComponent::GetMapConfig(
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
		Message = TEXT("FastGame: no world for Get Map Config");
		OnMapConfigFetched.Broadcast(false, TEXT(""), Message);
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
	UFastGameSubsystem* Subsystem = FastGameMapComponentUtil::ResolveSubsystem(this, Err);
	if (!Subsystem)
	{
		State->bSuccess = false;
		State->StatusCode = 0;
		State->Message = Err;
		State->bFinished = true;
		OnMapConfigFetched.Broadcast(false, TEXT(""), Err);
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
		OnMapConfigFetched.Broadcast(false, TEXT(""), LocalErr);
		return;
	}

	const FString ResolvedGame = FastGameMapComponentUtil::ResolveGameCode(Subsystem, GameCode);
	if (MapId.IsNone())
	{
		const FString LocalErr = TEXT("FastGame: MapId is not set");
		State->bSuccess = false;
		State->StatusCode = 0;
		State->Message = LocalErr;
		State->bFinished = true;
		OnMapConfigFetched.Broadcast(false, TEXT(""), LocalErr);
		return;
	}
	if (ResolvedGame.IsEmpty())
	{
		const FString LocalErr = TEXT("FastGame: GameCode is empty — call Initialize Game first");
		State->bSuccess = false;
		State->StatusCode = 0;
		State->Message = LocalErr;
		State->bFinished = true;
		OnMapConfigFetched.Broadcast(false, TEXT(""), LocalErr);
		return;
	}

	TWeakObjectPtr<UFastGameMapComponent> WeakThis(this);
	Client->Content->GetMapConfig(ResolvedGame, MapId.ToString(),
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
				if (UFastGameMapComponent* Self = WeakThis.Get())
				{
					Self->OnMapConfigFetched.Broadcast(bOk, Body, Msg);
				}
			});
		});
}

void UFastGameMapComponent::TravelMap(
	const FString& GameCode,
	FName TargetMapId,
	FLatentActionInfo LatentInfo,
	EFastGameTravelMapPin& Pin,
	int32& StatusCode,
	FString& Message,
	FFastGameBPSeatMint& Seat)
{
	Pin = EFastGameTravelMapPin::Failed;
	Seat = FFastGameBPSeatMint();
	StatusCode = 0;
	Message.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		Message = TEXT("FastGame: no world for Travel Map");
		return;
	}

	FLatentActionManager& LatentManager = World->GetLatentActionManager();
	if (LatentManager.FindExistingAction<FFastGameRequestLatentAction>(LatentInfo.CallbackTarget, LatentInfo.UUID))
	{
		return;
	}

	const TSharedRef<FFastGameRequestLatentState> State = MakeShared<FFastGameRequestLatentState>();
	FFastGameRequestLatentAction* Action = new FFastGameRequestLatentAction(LatentInfo, State);
	Action->StatusCodeOut = &StatusCode;
	Action->MessageOut = &Message;
	Action->TravelMapPinOut = &Pin;
	Action->SeatMintOut = &Seat;
	LatentManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, Action);

	FString Err;
	UFastGameSubsystem* Subsystem = FastGameMapComponentUtil::ResolveSubsystem(this, Err);
	if (!Subsystem)
	{
		FastGameMapComponentUtil::FinishTravelMap(State, EFastGameTravelMapPin::Failed, false, 0, Err);
		return;
	}

	TSharedPtr<FFastGameClient> Client = Subsystem->GetClient();
	if (!Client.IsValid())
	{
		const FString LocalErr = TEXT("FastGame: client not initialized");
		FastGameMapComponentUtil::FinishTravelMap(State, EFastGameTravelMapPin::Failed, false, 0, LocalErr);
		return;
	}

	const FString ResolvedGame = FastGameMapComponentUtil::ResolveGameCode(Subsystem, GameCode);
	if (ResolvedGame.IsEmpty())
	{
		const FString LocalErr = TEXT("FastGame: GameCode is empty — call Initialize Game first");
		FastGameMapComponentUtil::FinishTravelMap(State, EFastGameTravelMapPin::Failed, false, 0, LocalErr);
		return;
	}

	const FName ResolvedTarget = TargetMapId.IsNone() ? MapId : TargetMapId;
	if (ResolvedTarget.IsNone())
	{
		const FString LocalErr = TEXT("FastGame: TargetMapId is not set");
		FastGameMapComponentUtil::FinishTravelMap(State, EFastGameTravelMapPin::Failed, false, 0, LocalErr);
		return;
	}

	const FString TargetMap = ResolvedTarget.ToString();
	const FString Mode = FastGameMapComponentUtil::ResolveModeId(ModeId);
	const bool bOnline = FastGameMapComponentUtil::IsOnlineMode(Mode);
	const bool bSameMap = !MapId.IsNone() && TargetMap.Equals(MapId.ToString(), ESearchCase::IgnoreCase);

	TWeakObjectPtr<UFastGameMapComponent> WeakThis(this);

	auto FinishSeat = [State, bSameMap](bool bOk, const FFastGameSeatMint& NativeSeat, const FString& Error)
	{
		int32 Code = 0;
		FString Msg;
		FFastGameHttp::ParseStatusFromError(bOk, Error, Code, Msg);
		const FFastGameBPSeatMint BpSeat = FastGameBlueprintConvert::ToBP(NativeSeat);
		const EFastGameTravelMapPin OutPin = bOk
			? (bSameMap ? EFastGameTravelMapPin::WaitingHere : EFastGameTravelMapPin::Matchmaking)
			: EFastGameTravelMapPin::Failed;
		AsyncTask(ENamedThreads::GameThread, [State, bOk, Code, Msg, BpSeat, OutPin]()
		{
			FastGameMapComponentUtil::FinishTravelMap(State, OutPin, bOk, Code, Msg, BpSeat);
		});
	};

	if (bSameMap)
	{
		if (bOnline)
		{
			Client->Realtime->JoinMap(ResolvedGame, TargetMap, Mode, FinishSeat);
		}
		else
		{
			FastGameMapComponentUtil::FinishTravelMap(
				State, EFastGameTravelMapPin::WaitingHere, true, 200, TEXT(""));
		}
		return;
	}

	if (bOnline)
	{
		Client->Realtime->JoinMap(ResolvedGame, TargetMap, Mode, FinishSeat);
		return;
	}

	Client->Content->GetMapConfig(ResolvedGame, TargetMap,
		[WeakThis, State](bool bOk, TSharedPtr<FJsonObject> Json, FString Error)
		{
			const FString Body = FastGameBlueprintConvert::JsonObjectToString(Json);
			int32 Code = 0;
			FString Msg;
			FFastGameHttp::ParseStatusFromError(bOk, Error, Code, Msg);

			AsyncTask(ENamedThreads::GameThread, [WeakThis, State, bOk, Body, Code, Msg]()
			{
				if (!bOk)
				{
					FastGameMapComponentUtil::FinishTravelMap(
						State, EFastGameTravelMapPin::Failed, false, Code, Msg);
					return;
				}

				const FString EngineScene = FastGameMapComponentUtil::ExtractEngineSceneFromMapConfig(Body);
				if (EngineScene.IsEmpty())
				{
					const FString LocalErr = TEXT("FastGame: engine_scene is not configured for target map");
					FastGameMapComponentUtil::FinishTravelMap(
						State, EFastGameTravelMapPin::Failed, false, 0, LocalErr);
					return;
				}

				UFastGameMapComponent* Self = WeakThis.Get();
				if (!Self)
				{
					FastGameMapComponentUtil::FinishTravelMap(
						State, EFastGameTravelMapPin::Failed, false, 0,
						TEXT("FastGame: MapComponent destroyed during travel"));
					return;
				}

				UGameplayStatics::OpenLevel(Self, FName(*EngineScene));
				FastGameMapComponentUtil::FinishTravelMap(
					State, EFastGameTravelMapPin::Traveled, true, 200, TEXT(""));
			});
		});
}

void UFastGameMapComponent::NotifyQuestComplete(FName QuestId)
{
	OnQuestComplete.Broadcast(QuestId);
}

void UFastGameMapComponent::NotifyQuestFailed(FName QuestId)
{
	OnQuestFailed.Broadcast(QuestId);
}

void UFastGameMapComponent::NotifyQuestNotStartedYet(FName QuestId)
{
	OnQuestNotStartedYet.Broadcast(QuestId);
}
