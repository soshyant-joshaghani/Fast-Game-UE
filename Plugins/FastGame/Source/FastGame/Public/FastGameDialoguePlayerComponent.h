#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FastGameBlueprintTypes.h"
#include "FastGameDialoguePlayerComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFastGameDialogueSuccess, const FString&, DialogueId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFastGameDialogueFailed, const FString&, DialogueId, const FString&, Message);

/** Fetch and play a dialogue scenario (tip GetDialogue). Auth-style Success | Failed pins. */
UCLASS(ClassGroup = (FastGame), meta = (BlueprintSpawnableComponent))
class FASTGAME_API UFastGameDialoguePlayerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Dialogue")
	FName DialogueId;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Dialogue")
	FOnFastGameDialogueSuccess OnSuccess;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Dialogue")
	FOnFastGameDialogueFailed OnFailed;

	UFUNCTION(BlueprintCallable, Category = "FastGame|Dialogue", meta = (ExpandEnumAsExecs = "Outcome", DisplayName = "Play Dialogue"))
	void PlayDialogue(FName InDialogueId, EFastGameRequestOutcome& Outcome, FString& Message);
};
