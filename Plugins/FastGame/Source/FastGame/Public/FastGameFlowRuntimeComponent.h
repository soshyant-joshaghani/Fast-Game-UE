#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FastGameBlueprintTypes.h"
#include "FastGameFlowRuntimeComponent.generated.h"

class UFastGameMapComponent;
class UFastGameGameplayDirectorComponent;
class UFastGameDialoguePlayerComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFastGameFlowNode, FName, NodeId, FName, Kind);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFastGameFlowSimple);

/**
 * Walks compiled map_mode.flow_runtime (V3 hub).
 * NotifyTriggerEnter for Doctor / doors.
 */
UCLASS(ClassGroup = (FastGame), meta = (BlueprintSpawnableComponent, DisplayName = "Flow Runtime"))
class FASTGAME_API UFastGameFlowRuntimeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Flow")
	TObjectPtr<UFastGameMapComponent> Map;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Flow")
	TObjectPtr<UFastGameGameplayDirectorComponent> Director;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Flow")
	TObjectPtr<UFastGameDialoguePlayerComponent> Dialogue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Flow")
	bool bAutoStartLevelEvents = true;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Flow")
	FOnFastGameFlowNode OnNodeExecuted;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Flow")
	FOnFastGameDirectorMessage OnFlowFailed;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Flow")
	FOnFastGameFlowSimple OnFellowUnlocked;

	UFUNCTION(BlueprintCallable, Category = "FastGame|Flow", meta = (DisplayName = "Load Flow From Map Tip Json"))
	void LoadFromMapTipJson(const FString& JsonBody);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Flow", meta = (DisplayName = "Boot Level Flow Events"))
	void BootFromLoadedTip();

	UFUNCTION(BlueprintCallable, Category = "FastGame|Flow", meta = (DisplayName = "Notify Trigger Enter"))
	void NotifyTriggerEnter(FName TriggerId);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Flow", meta = (DisplayName = "Run Flow From Node"))
	void RunFromNode(FName NodeId);

protected:
	virtual void BeginPlay() override;

	TMap<FName, TSharedPtr<FJsonObject>> Nodes;
	TMap<FName, TSharedPtr<FJsonValue>> Locals;
	TSet<FName> DoOnceFired;
	FString MapId;
	bool bBusy = false;

	void ResolveRefs();
	void StartEventKinds(const TArray<FString>& Kinds);
	void Walk(FName StartNodeId);
	FName ExecuteNode(const TSharedPtr<FJsonObject>& Node, const FString& Kind);
	FName FirstNext(const TSharedPtr<FJsonObject>& Node) const;
	FName BranchNext(const TSharedPtr<FJsonObject>& Node, const FString& Pin) const;
	bool GetVarBool(const FString& Name) const;
	void SetVar(const FString& Name, bool bValue);
	void UnlockAchievements(const TArray<FString>& Ids);
	void TryUnlockFellow();
};
