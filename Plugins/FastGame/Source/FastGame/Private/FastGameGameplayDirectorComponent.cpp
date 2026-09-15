#include "FastGameGameplayDirectorComponent.h"
#include "FastGameMapComponent.h"
#include "FastGameCameraControllerComponent.h"
#include "FastGameCharacterControllerComponent.h"
#include "FastGameCameraRuntimeComponent.h"
#include "FastGameMovementRuntimeComponent.h"
#include "FastGameAbilityRuntimeComponent.h"
#include "FastGameParamRuntimeComponent.h"
#include "FastGameLootRuntimeComponent.h"
#include "FastGameCharacterComponent.h"
#include "FastGameFlowRuntimeComponent.h"
#include "FastGameSubsystem.h"
#include "FastGameClient.h"
#include "FastGameBlueprintConvert.h"
#include "FastGameHttp.h"
#include "FastGameLatentActions.h"
#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"

void UFastGameGameplayDirectorComponent::BeginPlay()
{
	Super::BeginPlay();
	ResolveModules();
}

void UFastGameGameplayDirectorComponent::ResolveModules()
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
	if (!CameraController)
	{
		CameraController = Owner->FindComponentByClass<UFastGameCameraControllerComponent>();
	}
	if (!CharacterController)
	{
		CharacterController = Owner->FindComponentByClass<UFastGameCharacterControllerComponent>();
	}
	if (!CameraRuntime)
	{
		CameraRuntime = Owner->FindComponentByClass<UFastGameCameraRuntimeComponent>();
	}
	if (!MovementRuntime)
	{
		MovementRuntime = Owner->FindComponentByClass<UFastGameMovementRuntimeComponent>();
	}
	if (!CameraController && CameraRuntime)
	{
		CameraController = CameraRuntime;
	}
	if (!CharacterController && MovementRuntime)
	{
		CharacterController = MovementRuntime;
	}
	if (!AbilityRuntime)
	{
		AbilityRuntime = Owner->FindComponentByClass<UFastGameAbilityRuntimeComponent>();
	}
	if (!ParamRuntime)
	{
		ParamRuntime = Owner->FindComponentByClass<UFastGameParamRuntimeComponent>();
	}
	if (!LootRuntime)
	{
		LootRuntime = Owner->FindComponentByClass<UFastGameLootRuntimeComponent>();
	}
	if (!FlowRuntime)
	{
		FlowRuntime = Owner->FindComponentByClass<UFastGameFlowRuntimeComponent>();
	}
	if (!PlayerEntity)
	{
		PlayerEntity = Owner->FindComponentByClass<UFastGameCharacterComponent>();
	}
	if (CameraController && !CameraController->FollowTarget && PlayerEntity)
	{
		CameraController->SetFollowTarget(PlayerEntity->GetOwner());
	}
	if (FlowRuntime)
	{
		FlowRuntime->Director = this;
		FlowRuntime->Map = Map;
	}
}

void UFastGameGameplayDirectorComponent::ApplyCameraProfile(FName Profile)
{
	ActiveCameraProfile = Profile;
	if (CameraController)
	{
		CameraController->ApplyCameraProfile(Profile);
	}
	else if (CameraRuntime)
	{
		CameraRuntime->ApplyCameraProfile(Profile);
	}
}

void UFastGameGameplayDirectorComponent::ApplyMovementProfile(FName Profile)
{
	ActiveMovementProfile = Profile;
	if (CharacterController)
	{
		CharacterController->ApplyMovementProfile(Profile);
	}
	else if (MovementRuntime)
	{
		MovementRuntime->ApplyMovementProfile(Profile);
	}
}

void UFastGameGameplayDirectorComponent::ActivateAbility(FName AbilityId)
{
	if (AbilityRuntime)
	{
		AbilityRuntime->ActivateAbility(AbilityId);
	}
}

void UFastGameGameplayDirectorComponent::SetAnimatorFloat(FName ParamName, float Value)
{
	if (ParamRuntime)
	{
		ParamRuntime->SetAnimatorFloat(ParamName, Value);
	}
}

void UFastGameGameplayDirectorComponent::SetAnimatorBool(FName ParamName, bool bValue)
{
	if (ParamRuntime)
	{
		ParamRuntime->SetAnimatorBool(ParamName, bValue);
	}
}

void UFastGameGameplayDirectorComponent::SetMaterialScalar(FName ParamName, float Value)
{
	if (ParamRuntime)
	{
		ParamRuntime->SetMaterialScalar(ParamName, Value);
	}
}

void UFastGameGameplayDirectorComponent::ApplyMapConfigJson(const FString& JsonBody)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonBody);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return;
	}
	const TSharedPtr<FJsonObject>* PayloadPtr = nullptr;
	const TSharedPtr<FJsonObject> Payload =
		Root->TryGetObjectField(TEXT("payload"), PayloadPtr) && PayloadPtr && PayloadPtr->IsValid()
			? *PayloadPtr
			: Root;

	FString Cam;
	if (Payload->TryGetStringField(TEXT("camera_profile"), Cam) && !Cam.IsEmpty())
	{
		ApplyCameraProfile(FName(*Cam));
	}
	FString InputId;
	if (Payload->TryGetStringField(TEXT("input_profile_id"), InputId) && !InputId.IsEmpty())
	{
		ActiveInputProfileId = FName(*InputId);
	}
	if (FlowRuntime)
	{
		FlowRuntime->LoadFromMapTipJson(JsonBody);
		FlowRuntime->BootFromLoadedTip();
	}
}

void UFastGameGameplayDirectorComponent::NotifyTriggerEnter(FName TriggerId)
{
	if (FlowRuntime)
	{
		FlowRuntime->NotifyTriggerEnter(TriggerId);
	}
}

void UFastGameGameplayDirectorComponent::BootGameplay(
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
	ResolveModules();

	UWorld* World = GetWorld();
	if (!World)
	{
		Message = TEXT("FastGame: no world for Boot Gameplay");
		OnBootFailed.Broadcast(Message);
		return;
	}
	if (!Map || Map->MapId.IsNone())
	{
		Message = TEXT("FastGame: Director needs MapId on Map component");
		OnBootFailed.Broadcast(Message);
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

	UGameInstance* GI = World->GetGameInstance();
	UFastGameSubsystem* Subsystem = GI ? GI->GetSubsystem<UFastGameSubsystem>() : nullptr;
	if (!Subsystem || !Subsystem->GetClient())
	{
		State->bSuccess = false;
		State->Message = TEXT("FastGame: subsystem/client missing — Initialize Game first");
		State->bFinished = true;
		OnBootFailed.Broadcast(State->Message);
		return;
	}

	FString ResolvedGame = GameCode.TrimStartAndEnd();
	if (ResolvedGame.IsEmpty())
	{
		ResolvedGame = Subsystem->GetGameCode().TrimStartAndEnd();
	}
	if (ResolvedGame.IsEmpty())
	{
		State->bSuccess = false;
		State->Message = TEXT("FastGame: GameCode is empty");
		State->bFinished = true;
		OnBootFailed.Broadcast(State->Message);
		return;
	}

	TWeakObjectPtr<UFastGameGameplayDirectorComponent> WeakThis(this);
	const FString MapIdStr = Map->MapId.ToString();
	Subsystem->GetClient()->Content->GetMapConfig(
		ResolvedGame,
		MapIdStr,
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
				if (UFastGameGameplayDirectorComponent* Self = WeakThis.Get())
				{
					if (bOk)
					{
						Self->ApplyMapConfigJson(Body);
						Self->OnBootComplete.Broadcast(true, Body, TEXT(""));
					}
					else
					{
						Self->OnBootFailed.Broadcast(Msg);
						Self->OnBootComplete.Broadcast(false, TEXT(""), Msg);
					}
				}
				State->bFinished = true;
			});
		});
}
