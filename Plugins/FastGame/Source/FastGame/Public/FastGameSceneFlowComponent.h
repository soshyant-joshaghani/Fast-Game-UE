#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FastGameSceneFlowComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFastGameSceneComplete, FName, SceneName);

/**
 * One main scene controller per map (BP_0_SPLASH … BP_5_LEVEL).
 * Set NextScene, call CompleteScene when done — mirrors Unity FastGameSceneFlowBehaviour.
 */
UCLASS(ClassGroup = (FastGame), meta = (BlueprintSpawnableComponent))
class FASTGAME_API UFastGameSceneFlowComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFastGameSceneFlowComponent();

	/** This scene NAME (MAP_0_SPLASH, MAP_2_AUTH, …). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Scene")
	FName SceneName;

	/** Level to open after CompleteScene when bAutoOpenNextOnComplete is true. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Scene")
	FName NextScene;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Scene")
	bool bAutoOpenNextOnComplete = true;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Scene")
	FOnFastGameSceneComplete OnSceneComplete;

	UFUNCTION(BlueprintCallable, Category = "FastGame|Scene")
	void CompleteScene();

	UFUNCTION(BlueprintCallable, Category = "FastGame|Scene")
	void OpenNextScene();

	UFUNCTION(BlueprintCallable, Category = "FastGame|Scene")
	void OpenScene(FName LevelName);
};
