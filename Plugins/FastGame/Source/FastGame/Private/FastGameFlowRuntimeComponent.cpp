#include "FastGameFlowRuntimeComponent.h"
#include "FastGameMapComponent.h"
#include "FastGameGameplayDirectorComponent.h"
#include "FastGameDialoguePlayerComponent.h"
#include "FastGameSubsystem.h"
#include "FastGameClient.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"
#include "Engine/LatentActionManager.h"
#include "Async/Async.h"

void UFastGameFlowRuntimeComponent::BeginPlay()
{
	Super::BeginPlay();
	ResolveRefs();
}

void UFastGameFlowRuntimeComponent::ResolveRefs()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	if (!Director)
	{
		Director = Owner->FindComponentByClass<UFastGameGameplayDirectorComponent>();
	}
	if (!Map && Director)
	{
		Map = Director->Map;
	}
	if (!Map)
	{
		Map = Owner->FindComponentByClass<UFastGameMapComponent>();
	}
	if (!Dialogue)
	{
		Dialogue = Owner->FindComponentByClass<UFastGameDialoguePlayerComponent>();
	}
}

void UFastGameFlowRuntimeComponent::LoadFromMapTipJson(const FString& JsonBody)
{
	Nodes.Reset();
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
	Payload->TryGetStringField(TEXT("map_id"), MapId);

	const TArray<TSharedPtr<FJsonValue>>* Runtime = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Modes = nullptr;
	if (Payload->TryGetArrayField(TEXT("map_modes"), Modes) && Modes && Map)
	{
		const FString ModeId = Map->ModeId.ToString();
		for (const TSharedPtr<FJsonValue>& V : *Modes)
		{
			const TSharedPtr<FJsonObject>* MM = nullptr;
			if (!V.IsValid() || !V->TryGetObject(MM) || !MM || !MM->IsValid())
			{
				continue;
			}
			FString Mid;
			(*MM)->TryGetStringField(TEXT("mode_id"), Mid);
			if (!ModeId.IsEmpty() && Mid != ModeId)
			{
				continue;
			}
			(*MM)->TryGetArrayField(TEXT("flow_runtime"), Runtime);
			if (Runtime)
			{
				break;
			}
		}
	}
	if (!Runtime)
	{
		Payload->TryGetArrayField(TEXT("flow_runtime"), Runtime);
	}
	if (!Runtime)
	{
		return;
	}
	for (const TSharedPtr<FJsonValue>& V : *Runtime)
	{
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		if (!V.IsValid() || !V->TryGetObject(Obj) || !Obj || !Obj->IsValid())
		{
			continue;
		}
		FString NodeId;
		if ((*Obj)->TryGetStringField(TEXT("node_id"), NodeId) && !NodeId.IsEmpty())
		{
			Nodes.Add(FName(*NodeId), *Obj);
		}
	}
}

void UFastGameFlowRuntimeComponent::BootFromLoadedTip()
{
	if (!bAutoStartLevelEvents)
	{
		return;
	}
	StartEventKinds({TEXT("on_level_started"), TEXT("on_start"), TEXT("on_game_enter")});
}

void UFastGameFlowRuntimeComponent::NotifyTriggerEnter(FName TriggerId)
{
	if (TriggerId.IsNone())
	{
		return;
	}
	const FString Want = TriggerId.ToString();
	for (const TPair<FName, TSharedPtr<FJsonObject>>& Pair : Nodes)
	{
		FString Kind;
		Pair.Value->TryGetStringField(TEXT("kind"), Kind);
		Kind = Kind.ToLower();
		if (Kind != TEXT("on_trigger_enter") && Kind != TEXT("on_interact"))
		{
			continue;
		}
		const TSharedPtr<FJsonObject>* Params = nullptr;
		FString Tid;
		if (Pair.Value->TryGetObjectField(TEXT("params"), Params) && Params && Params->IsValid())
		{
			(*Params)->TryGetStringField(TEXT("trigger_id"), Tid);
			if (Tid.IsEmpty())
			{
				(*Params)->TryGetStringField(TEXT("placement_id"), Tid);
			}
		}
		if (Tid.Equals(Want, ESearchCase::IgnoreCase))
		{
			RunFromNode(Pair.Key);
		}
	}
}

void UFastGameFlowRuntimeComponent::RunFromNode(FName NodeId)
{
	if (bBusy || NodeId.IsNone())
	{
		return;
	}
	bBusy = true;
	ResolveRefs();
	Walk(NodeId);
	bBusy = false;
}

void UFastGameFlowRuntimeComponent::StartEventKinds(const TArray<FString>& Kinds)
{
	for (const TPair<FName, TSharedPtr<FJsonObject>>& Pair : Nodes)
	{
		FString Kind;
		Pair.Value->TryGetStringField(TEXT("kind"), Kind);
		Kind = Kind.ToLower();
		for (const FString& Want : Kinds)
		{
			if (Kind == Want.ToLower())
			{
				RunFromNode(Pair.Key);
				break;
			}
		}
	}
}

void UFastGameFlowRuntimeComponent::Walk(FName StartNodeId)
{
	FName Current = StartNodeId;
	int32 Guard = 0;
	while (!Current.IsNone() && Guard++ < 256)
	{
		const TSharedPtr<FJsonObject>* NodePtr = Nodes.Find(Current);
		if (!NodePtr || !NodePtr->IsValid())
		{
			break;
		}
		FString Kind;
		(*NodePtr)->TryGetStringField(TEXT("kind"), Kind);
		OnNodeExecuted.Broadcast(Current, FName(*Kind));
		Current = ExecuteNode(*NodePtr, Kind.ToLower());
	}
}

FName UFastGameFlowRuntimeComponent::ExecuteNode(const TSharedPtr<FJsonObject>& Node, const FString& Kind)
{
	const TSharedPtr<FJsonObject>* ParamsPtr = nullptr;
	TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
	if (Node->TryGetObjectField(TEXT("params"), ParamsPtr) && ParamsPtr && ParamsPtr->IsValid())
	{
		Params = *ParamsPtr;
	}

	if (Kind == TEXT("set_var"))
	{
		FString Name;
		Params->TryGetStringField(TEXT("var_name"), Name);
		bool bVal = true;
		Params->TryGetBoolField(TEXT("value"), bVal);
		SetVar(Name, bVal);
		return FirstNext(Node);
	}
	if (Kind == TEXT("branch"))
	{
		FString Cond;
		Params->TryGetStringField(TEXT("condition_var"), Cond);
		bool bEquals = true;
		Params->TryGetBoolField(TEXT("equals"), bEquals);
		const bool bVal = GetVarBool(Cond);
		return BranchNext(Node, (bVal == bEquals) ? TEXT("true") : TEXT("false"));
	}
	if (Kind == TEXT("do_once"))
	{
		FString NodeId;
		Node->TryGetStringField(TEXT("node_id"), NodeId);
		const FName Id(*NodeId);
		if (DoOnceFired.Contains(Id))
		{
			return NAME_None;
		}
		DoOnceFired.Add(Id);
		const FName Done = BranchNext(Node, TEXT("completed"));
		return Done.IsNone() ? FirstNext(Node) : Done;
	}
	if (Kind == TEXT("play_dialogue") || Kind == TEXT("start_dialogue"))
	{
		FString DialogueId;
		Params->TryGetStringField(TEXT("dialogue_id"), DialogueId);
		if (Dialogue)
		{
			EFastGameRequestOutcome Outcome;
			FString Message;
			Dialogue->PlayDialogue(FName(*DialogueId), Outcome, Message);
		}
		TryUnlockFellow();
		return FirstNext(Node);
	}
	if (Kind == TEXT("load_level") || Kind == TEXT("travel_map"))
	{
		FString TargetMap;
		Params->TryGetStringField(TEXT("map_id"), TargetMap);
		if (Map && !TargetMap.IsEmpty())
		{
			EFastGameTravelMapPin Pin = EFastGameTravelMapPin::Failed;
			int32 Status = 0;
			FString Msg;
			FFastGameBPSeatMint Seat;
			FLatentActionInfo Latent;
			Latent.CallbackTarget = this;
			Latent.UUID = GetTypeHash(TargetMap);
			Latent.Linkage = 0;
			Latent.ExecutionFunction = NAME_None;
			Map->TravelMap(TEXT(""), FName(*TargetMap), Latent, Pin, Status, Msg, Seat);
		}
		const FName Traveled = BranchNext(Node, TEXT("traveled"));
		return Traveled.IsNone() ? FirstNext(Node) : Traveled;
	}
	if (Kind == TEXT("unlock_achievements"))
	{
		TArray<FString> Ids;
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (Params->TryGetArrayField(TEXT("achievement_ids"), Arr) && Arr)
		{
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				FString S;
				if (V.IsValid() && V->TryGetString(S) && !S.IsEmpty())
				{
					Ids.Add(S);
				}
			}
		}
		FString Single;
		if (Params->TryGetStringField(TEXT("achievement_id"), Single) && !Single.IsEmpty())
		{
			Ids.Add(Single);
		}
		UnlockAchievements(Ids);
		return FirstNext(Node);
	}
	if (Kind == TEXT("set_camera_profile") && Director)
	{
		FString Profile;
		Params->TryGetStringField(TEXT("profile"), Profile);
		if (Profile.IsEmpty())
		{
			Params->TryGetStringField(TEXT("camera_profile"), Profile);
		}
		Director->ApplyCameraProfile(FName(*Profile));
		return FirstNext(Node);
	}
	if (Kind.StartsWith(TEXT("literal_")) || Kind == TEXT("get_var") || Kind == TEXT("compare"))
	{
		return NAME_None;
	}
	return FirstNext(Node);
}

FName UFastGameFlowRuntimeComponent::FirstNext(const TSharedPtr<FJsonObject>& Node) const
{
	const TArray<TSharedPtr<FJsonValue>>* Next = nullptr;
	if (Node->TryGetArrayField(TEXT("next"), Next) && Next && Next->Num() > 0)
	{
		FString S;
		if ((*Next)[0].IsValid() && (*Next)[0]->TryGetString(S))
		{
			return FName(*S);
		}
	}
	return BranchNext(Node, TEXT("exec_out"));
}

FName UFastGameFlowRuntimeComponent::BranchNext(const TSharedPtr<FJsonObject>& Node, const FString& Pin) const
{
	const TSharedPtr<FJsonObject>* Branches = nullptr;
	if (!Node->TryGetObjectField(TEXT("branches"), Branches) || !Branches || !Branches->IsValid())
	{
		return NAME_None;
	}
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (!(*Branches)->TryGetArrayField(Pin, Arr) || !Arr || Arr->Num() == 0)
	{
		return NAME_None;
	}
	FString S;
	if ((*Arr)[0].IsValid() && (*Arr)[0]->TryGetString(S))
	{
		return FName(*S);
	}
	return NAME_None;
}

bool UFastGameFlowRuntimeComponent::GetVarBool(const FString& Name) const
{
	if (Name.IsEmpty())
	{
		return false;
	}
	if (const TSharedPtr<FJsonValue>* Found = Locals.Find(FName(*Name)))
	{
		if (Found->IsValid())
		{
			return (*Found)->AsBool();
		}
	}
	return false;
}

void UFastGameFlowRuntimeComponent::SetVar(const FString& Name, bool bValue)
{
	if (Name.IsEmpty())
	{
		return;
	}
	Locals.Add(FName(*Name), MakeShared<FJsonValueBoolean>(bValue));
	// Progress persist is best-effort via Dialogue player flags for V3; Flow locals cover Branch in-session.
}

void UFastGameFlowRuntimeComponent::UnlockAchievements(const TArray<FString>& Ids)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	UGameInstance* GI = World->GetGameInstance();
	if (!GI)
	{
		return;
	}
	UFastGameSubsystem* Sub = GI->GetSubsystem<UFastGameSubsystem>();
	if (!Sub || !Sub->IsInitialized() || !Sub->GetClient().IsValid())
	{
		return;
	}
	const FString GameCode = Sub->GetGameCode();
	TSharedPtr<FFastGameClient> Client = Sub->GetClient();
	for (const FString& Id : Ids)
	{
		TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("achievement_id"), Id);
		Client->Progress->Save(
			GameCode,
			TEXT("achievement.unlock"),
			MapId,
			Payload,
			[](bool /*bOk*/, TSharedPtr<FJsonObject> /*Json*/, FString /*Err*/) {});
	}
}

void UFastGameFlowRuntimeComponent::TryUnlockFellow()
{
	if (GetVarBool(TEXT("AskBallistix")) && GetVarBool(TEXT("AskEverything"))
		&& GetVarBool(TEXT("AskBash")) && GetVarBool(TEXT("AskProgress")))
	{
		UnlockAchievements({TEXT("ACH_FASTGAME_FELLOW")});
		OnFellowUnlocked.Broadcast();
	}
}
