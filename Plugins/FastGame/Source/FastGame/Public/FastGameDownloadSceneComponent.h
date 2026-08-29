#pragma once

#include "CoreMinimal.h"
#include "FastGameSceneFlowComponent.h"
#include "FastGameDownloadSceneComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFastGameDownloadProgress, float, Normalized, const FString&, Message);

/**
 * DOWNLOAD — fetches published tip pack index, filters quality × platform × language,
 * downloads to Saved/FastGame/packs, then opens NextScene.
 */
UCLASS(ClassGroup = (FastGame), meta = (BlueprintSpawnableComponent))
class FASTGAME_API UFastGameDownloadSceneComponent : public UFastGameSceneFlowComponent
{
	GENERATED_BODY()

public:
	UFastGameDownloadSceneComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Download")
	bool bAutoStart = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Download")
	bool bAdvanceWhenNothingToDownload = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Download")
	bool bSkipSplashPacks = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Download")
	float MinDisplaySeconds = 0.f;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Download")
	FOnFastGameDownloadProgress OnDownloadProgress;

	UFUNCTION(BlueprintCallable, Category = "FastGame|Download")
	void RunDownload();

	UFUNCTION(BlueprintCallable, Category = "FastGame|Download")
	void SetProgress(float Normalized, const FString& Message);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Download")
	void FinishDownload();

protected:
	virtual void BeginPlay() override;

private:
	bool IsTipNotPublished(const FString& Error) const;

	bool bCompleted = false;
	double StartedAt = 0.0;
};
