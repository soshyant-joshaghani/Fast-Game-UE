#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FastGameAchievementComponent.generated.h"

/**
 * Binds a catalog achievement NAME on a prefab (trophy / inspect UI hook).
 */
UCLASS(ClassGroup = (FastGame), meta = (BlueprintSpawnableComponent))
class FASTGAME_API UFastGameAchievementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Achievement")
	FName AchievementId;

	UFUNCTION(BlueprintPure, Category = "FastGame|Achievement", meta = (DisplayName = "Get Achievement Id"))
	FName GetAchievementId() const { return AchievementId; }
};
