#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FastGameAvatarComponent.generated.h"

/**
 * Binds a catalog avatar NAME on a prefab (portrait / collectibles UI).
 * Ownership comes from Progress / Shop — display uses local project assets.
 */
UCLASS(ClassGroup = (FastGame), meta = (BlueprintSpawnableComponent))
class FASTGAME_API UFastGameAvatarComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Avatar")
	FName AvatarId;

	UFUNCTION(BlueprintPure, Category = "FastGame|Avatar", meta = (DisplayName = "Get Avatar Id"))
	FName GetAvatarId() const { return AvatarId; }
};
