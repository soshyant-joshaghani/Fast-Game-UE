#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FastGameBlueprintTypes.h"
#include "FastGameParamRuntimeComponent.generated.h"

class USkeletalMeshComponent;
class UMeshComponent;

/**
 * Applies tip / Flow SetAnimator · SetMaterial writes onto this actor's art (G1).
 * Declared param NAMEs must exist on AnimInstance / materials.
 */
UCLASS(ClassGroup = (FastGame), meta = (BlueprintSpawnableComponent))
class FASTGAME_API UFastGameParamRuntimeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Params")
	TObjectPtr<USkeletalMeshComponent> TargetSkeletalMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Params")
	TObjectPtr<UMeshComponent> TargetMesh;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Params")
	FOnFastGameParamWritten OnParamWritten;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Params")
	FOnFastGameDirectorMessage OnParamFailed;

	UFUNCTION(BlueprintCallable, Category = "FastGame|Params", meta = (DisplayName = "Set Animator Float"))
	bool SetAnimatorFloat(FName ParamName, float Value);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Params", meta = (DisplayName = "Set Animator Bool"))
	bool SetAnimatorBool(FName ParamName, bool bValue);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Params", meta = (DisplayName = "Set Animator Trigger"))
	bool SetAnimatorTrigger(FName ParamName);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Params", meta = (DisplayName = "Set Material Scalar"))
	bool SetMaterialScalar(FName ParamName, float Value);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Params", meta = (DisplayName = "Apply Param Write"))
	bool ApplyWrite(const FFastGameBPParamWrite& Write);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Params", meta = (DisplayName = "Apply Param Writes"))
	void ApplyWrites(const TArray<FFastGameBPParamWrite>& Writes);

protected:
	virtual void BeginPlay() override;

	void ResolveTargets();
};
