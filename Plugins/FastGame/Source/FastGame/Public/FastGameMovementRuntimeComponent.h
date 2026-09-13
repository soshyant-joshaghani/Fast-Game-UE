#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FastGameMovementRuntimeComponent.generated.h"

/** Tip movement_profile → AddMovementInput on owner pawn (G1). */
UCLASS(ClassGroup = (FastGame), meta = (BlueprintSpawnableComponent))
class FASTGAME_API UFastGameMovementRuntimeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Movement")
	FName MovementProfile = TEXT("humanoid");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Movement")
	float WalkSpeedScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Movement")
	bool bEnablePlayerInput = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Movement")
	FName SpeedParamName = TEXT("Speed");

	UFUNCTION(BlueprintCallable, Category = "FastGame|Movement", meta = (DisplayName = "Apply Movement Profile"))
	void ApplyMovementProfile(FName Profile);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFastGameMovementRuntimeComponent();
};
