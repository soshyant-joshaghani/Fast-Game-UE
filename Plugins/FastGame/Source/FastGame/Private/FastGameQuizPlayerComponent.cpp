#include "FastGameQuizPlayerComponent.h"
#include "FastGameSubsystem.h"

void UFastGameQuizPlayerComponent::PlayQuiz(
	FName InQuizId,
	EFastGameRequestOutcome& Outcome,
	FString& Message)
{
	Outcome = EFastGameRequestOutcome::Failed;
	Message = TEXT("");
	const FName Id = InQuizId.IsNone() ? QuizId : InQuizId;
	if (Id.IsNone())
	{
		Message = TEXT("QuizId required");
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
	Outcome = EFastGameRequestOutcome::Success;
	OnSuccess.Broadcast(Id.ToString());
}
