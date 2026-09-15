#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FastGameCharacterControllerComponent.generated.h"

class UFastGameParamRuntimeComponent;
class UFastGameAbilityRuntimeComponent;
class UCharacterMovementComponent;

/**
 * Owned humanoid locomotion (V2): move / jump N / stance + tip Speed bind.
 * Profiles: humanoid (default), fly. Vehicle_* reserved for V4.
 */
UCLASS(ClassGroup = (FastGame), meta = (BlueprintSpawnableComponent, DisplayName = "Character Controller"))
class FASTGAME_API UFastGameCharacterControllerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Character")
	FName MovementProfile = TEXT("humanoid");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Character")
	float WalkSpeed = 450.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Character")
	float SprintMultiplier = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Character")
	float CrouchMultiplier = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Character")
	float JumpZVelocity = 700.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Character")
	int32 JumpCount = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Character")
	bool bEnablePlayerInput = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Character")
	FName SpeedParamName = TEXT("Speed");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Character")
	FName CrouchParamName = TEXT("IsCrouching");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Character")
	FName SprintParamName = TEXT("IsSprinting");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Character")
	TObjectPtr<UFastGameParamRuntimeComponent> Params;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Character")
	TObjectPtr<UFastGameAbilityRuntimeComponent> Abilities;

	UFUNCTION(BlueprintCallable, Category = "FastGame|Character", meta = (DisplayName = "Apply Movement Profile"))
	void ApplyMovementProfile(FName Profile);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Character", meta = (DisplayName = "Apply Locomotion From Tip Json"))
	void ApplyLocomotionFromTipJson(const FString& JsonBody);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Character", meta = (DisplayName = "Set Sprint"))
	void SetSprint(bool bSprint);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Character", meta = (DisplayName = "Set Crouch"))
	void SetCrouch(bool bCrouch);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Character", meta = (DisplayName = "Request Jump"))
	void RequestJump();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFastGameCharacterControllerComponent();

protected:
	virtual void BeginPlay() override;

	bool bSprint = false;
	bool bCrouch = false;
	int32 JumpsLeft = 2;

	void ResolveRefs();
	UCharacterMovementComponent* GetMovement() const;
};
