#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FastGameCameraControllerComponent.generated.h"

class UCameraComponent;

/**
 * Tip camera_profile → tps / fps / top_down / side (V2).
 * Aliases: tps_follow → tps.
 */
UCLASS(ClassGroup = (FastGame), meta = (BlueprintSpawnableComponent, DisplayName = "Camera Controller"))
class FASTGAME_API UFastGameCameraControllerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Camera")
	TObjectPtr<UCameraComponent> RigCamera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Camera")
	TObjectPtr<AActor> FollowTarget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Camera")
	FName CameraProfile = TEXT("tps");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Camera")
	FVector TpsOffset = FVector(-450.f, 0.f, 220.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Camera")
	FVector TopDownOffset = FVector(0.f, 0.f, 1800.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Camera")
	FVector SideOffset = FVector(0.f, -800.f, 160.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Camera")
	float FollowInterpSpeed = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Camera")
	TArray<FName> AllowedCameras;

	UFUNCTION(BlueprintCallable, Category = "FastGame|Camera", meta = (DisplayName = "Apply Camera Profile"))
	void ApplyCameraProfile(FName Profile);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Camera", meta = (DisplayName = "Set Follow Target"))
	void SetFollowTarget(AActor* Target);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Camera", meta = (DisplayName = "Apply Allowed Cameras From Tip Json"))
	void ApplyAllowedCamerasFromTipJson(const FString& JsonBody);

	UFUNCTION(BlueprintPure, Category = "FastGame|Camera", meta = (DisplayName = "Normalize Camera Profile"))
	static FName NormalizeCameraProfile(FName Profile);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

	UFastGameCameraControllerComponent();
};
