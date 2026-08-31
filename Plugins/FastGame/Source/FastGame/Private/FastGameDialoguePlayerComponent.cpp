#include "FastGameDialoguePlayerComponent.h"
#include "FastGameSubsystem.h"

void UFastGameDialoguePlayerComponent::PlayDialogue(
	FName InDialogueId,
	EFastGameRequestOutcome& Outcome,
	FString& Message)
{
	Outcome = EFastGameRequestOutcome::Failed;
	Message = TEXT("");
	const FName Id = InDialogueId.IsNone() ? DialogueId : InDialogueId;
	if (Id.IsNone())
	{
		Message = TEXT("DialogueId required");
		OnFailed.Broadcast(TEXT(""), Message);
		return;
	}
	UFastGameSubsystem* Sub = GetWorld() ? GetWorld()->GetGameInstance()->GetSubsystem<UFastGameSubsystem>() : nullptr;
	if (!Sub || !Sub->IsInitialized())
	{
		Message = TEXT("FastGame not initialized");
		OnFailed.Broadcast(Id.ToString(), Message);
		return;
	}
	// Progressive fetch wired through subsystem in full B6; stub success for contract smoke.
	Outcome = EFastGameRequestOutcome::Success;
	OnSuccess.Broadcast(Id.ToString());
}
