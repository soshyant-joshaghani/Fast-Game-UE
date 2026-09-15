#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FastGameBlueprintTypes.h"
#include "FastGameAbilityRuntimeComponent.generated.h"

class UFastGameParamRuntimeComponent;

/** Tip-fed ability activate → ParamRuntime writes (G1). */
UCLASS(ClassGroup = (FastGame), meta = (BlueprintSpawnableComponent))
class FASTGAME_API UFastGameAbilityRuntimeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Ability")
	TObjectPtr<UFastGameParamRuntimeComponent> Params;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Ability")
	TArray<FFastGameBPAbilityDef> Abilities;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Ability")
	FOnFastGameAbilityPin OnAbilityActivated;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Ability")
	FOnFastGameAbilityPin OnAbilityDeactivated;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Ability")
	FOnFastGameDirectorMessage OnAbilityFailed;

	UFUNCTION(BlueprintCallable, Category = "FastGame|Ability", meta = (DisplayName = "Activate Ability"))
	void ActivateAbility(FName AbilityId);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Ability", meta = (DisplayName = "Deactivate Ability"))
	void DeactivateAbility(FName AbilityId);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Ability", meta = (DisplayName = "Is Ability Active"))
	bool IsAbilityActive(FName AbilityId) const;

	UFUNCTION(BlueprintCallable, Category = "FastGame|Ability", meta = (DisplayName = "Load Abilities From Tip Json"))
	void LoadAbilitiesFromTipJson(const FString& JsonBody);

protected:
	virtual void BeginPlay() override;

	TSet<FName> ActiveAbilities;

	const FFastGameBPAbilityDef* FindDef(FName AbilityId) const;
};
