#include "FastGameDialoguePlayerComponent.h"
#include "FastGameSubsystem.h"
#include "FastGameClient.h"
#include "Engine/GameInstance.h"
#include "Async/Async.h"

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
	UWorld* World = GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	UFastGameSubsystem* Sub = GI ? GI->GetSubsystem<UFastGameSubsystem>() : nullptr;
	if (!Sub || !Sub->IsInitialized() || !Sub->GetClient().IsValid())
	{
		Message = TEXT("FastGame not initialized");
		OnFailed.Broadcast(Id.ToString(), Message);
		return;
	}

	const FString GameCode = Sub->GetGameCode();
	TWeakObjectPtr<UFastGameDialoguePlayerComponent> WeakThis(this);
	const FString DialogueStr = Id.ToString();
	Sub->GetClient()->Content->GetDialogue(
		GameCode,
		DialogueStr,
		[WeakThis, DialogueStr](bool bOk, TSharedPtr<FJsonObject> /*Json*/, FString Error)
		{
			AsyncTask(ENamedThreads::GameThread, [WeakThis, DialogueStr, bOk, Error]()
			{
				if (UFastGameDialoguePlayerComponent* Self = WeakThis.Get())
				{
					if (bOk)
					{
						// Full choice walk is Blueprint-driven; tip fetch success = playable for V3 Flow.
						Self->OnSuccess.Broadcast(DialogueStr);
					}
					else
					{
						Self->OnFailed.Broadcast(DialogueStr, Error);
					}
				}
			});
		});

	// Latent-less Blueprint pin: Success means request accepted (async fetch continues).
	Outcome = EFastGameRequestOutcome::Success;
	Message = TEXT("");
}
