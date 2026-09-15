#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/LatentActionManager.h"
#include "FastGameBlueprintTypes.h"
#include "FastGameGameplayDirectorComponent.generated.h"

class UFastGameMapComponent;
class UFastGameCameraControllerComponent;
class UFastGameCharacterControllerComponent;
class UFastGameCameraRuntimeComponent;
class UFastGameMovementRuntimeComponent;
class UFastGameAbilityRuntimeComponent;
class UFastGameParamRuntimeComponent;
class UFastGameLootRuntimeComponent;
class UFastGameCharacterComponent;
class UFastGameFlowRuntimeComponent;

/**
 * Single LEVEL façade. Boots tip map profiles; owns V2 Character/Camera controllers.
 */
UCLASS(ClassGroup = (FastGame), meta = (BlueprintSpawnableComponent))
class FASTGAME_API UFastGameGameplayDirectorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Director")
	TObjectPtr<UFastGameMapComponent> Map;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Director")
	TObjectPtr<UFastGameCameraControllerComponent> CameraController;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Director")
	TObjectPtr<UFastGameCharacterControllerComponent> CharacterController;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Director")
	TObjectPtr<UFastGameCameraRuntimeComponent> CameraRuntime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Director")
	TObjectPtr<UFastGameMovementRuntimeComponent> MovementRuntime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Director")
	TObjectPtr<UFastGameAbilityRuntimeComponent> AbilityRuntime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Director")
	TObjectPtr<UFastGameParamRuntimeComponent> ParamRuntime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Director")
	TObjectPtr<UFastGameLootRuntimeComponent> LootRuntime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Director")
	TObjectPtr<UFastGameFlowRuntimeComponent> FlowRuntime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Director")
	TObjectPtr<UFastGameCharacterComponent> PlayerEntity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Director")
	bool bBootOnBeginPlay = true;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame|Director")
	FName ActiveCameraProfile;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame|Director")
	FName ActiveMovementProfile;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame|Director")
	FName ActiveInputProfileId;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Director")
	FOnFastGameMapConfigFetched OnBootComplete;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Director")
	FOnFastGameDirectorMessage OnBootFailed;

	UFUNCTION(BlueprintCallable, Category = "FastGame|Director", meta = (Latent, LatentInfo = "LatentInfo", DisplayName = "Boot Gameplay", CPP_Default_GameCode = ""))
	void BootGameplay(
		const FString& GameCode,
		FLatentActionInfo LatentInfo,
		bool& bSuccess,
		int32& StatusCode,
		FString& Message,
		FString& JsonBody);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Director", meta = (DisplayName = "Apply Camera Profile"))
	void ApplyCameraProfile(FName Profile);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Director", meta = (DisplayName = "Apply Movement Profile"))
	void ApplyMovementProfile(FName Profile);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Director", meta = (DisplayName = "Activate Ability"))
	void ActivateAbility(FName AbilityId);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Director", meta = (DisplayName = "Set Animator Float"))
	void SetAnimatorFloat(FName ParamName, float Value);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Director", meta = (DisplayName = "Set Animator Bool"))
	void SetAnimatorBool(FName ParamName, bool bValue);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Director", meta = (DisplayName = "Set Material Scalar"))
	void SetMaterialScalar(FName ParamName, float Value);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Director", meta = (DisplayName = "Notify Trigger Enter"))
	void NotifyTriggerEnter(FName TriggerId);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Director", meta = (DisplayName = "Apply Map Config Json"))
	void ApplyMapConfigJson(const FString& JsonBody);

protected:
	virtual void BeginPlay() override;

	void ResolveModules();
};
