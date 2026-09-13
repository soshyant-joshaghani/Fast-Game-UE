#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FastGameCameraRuntimeComponent.generated.h"

class UCameraComponent;

/** Tip camera_profile → follow / top-down / fps (G1). */
UCLASS(ClassGroup = (FastGame), meta = (BlueprintSpawnableComponent))
class FASTGAME_API UFastGameCameraRuntimeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Camera")
	TObjectPtr<UCameraComponent> RigCamera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Camera")
	TObjectPtr<AActor> FollowTarget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Camera")
	FName CameraProfile = TEXT("tps_follow");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Camera")
	FVector TpsOffset = FVector(-450.f, 0.f, 220.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Camera")
	FVector TopDownOffset = FVector(0.f, 0.f, 1800.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Camera")
	float FollowInterpSpeed = 12.f;

	UFUNCTION(BlueprintCallable, Category = "FastGame|Camera", meta = (DisplayName = "Apply Camera Profile"))
	void ApplyCameraProfile(FName Profile);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Camera", meta = (DisplayName = "Set Follow Target"))
	void SetFollowTarget(AActor* Target);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

	UFastGameCameraRuntimeComponent();
};
