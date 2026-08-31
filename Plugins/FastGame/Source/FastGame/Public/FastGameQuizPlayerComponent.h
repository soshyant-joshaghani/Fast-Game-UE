#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FastGameBlueprintTypes.h"
#include "FastGameQuizPlayerComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFastGameQuizSuccess, const FString&, QuizId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFastGameQuizFailed, const FString&, QuizId, const FString&, Message);

/** Fetch and play a quiz scenario (tip GetQuiz + Grade). Auth-style Success | Failed pins. */
UCLASS(ClassGroup = (FastGame), meta = (BlueprintSpawnableComponent))
class FASTGAME_API UFastGameQuizPlayerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Quiz")
	FName QuizId;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Quiz")
	FOnFastGameQuizSuccess OnSuccess;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Quiz")
	FOnFastGameQuizFailed OnFailed;

	UFUNCTION(BlueprintCallable, Category = "FastGame|Quiz", meta = (ExpandEnumAsExecs = "Outcome", DisplayName = "Play Quiz"))
	void PlayQuiz(FName InQuizId, EFastGameRequestOutcome& Outcome, FString& Message);
};
