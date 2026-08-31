#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FastGameTitleComponent.generated.h"

/**
 * Binds a catalog title NAME on a prefab (profile / menu display).
 */
UCLASS(ClassGroup = (FastGame), meta = (BlueprintSpawnableComponent))
class FASTGAME_API UFastGameTitleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Title")
	FName TitleId;

	UFUNCTION(BlueprintPure, Category = "FastGame|Title", meta = (DisplayName = "Get Title Id"))
	FName GetTitleId() const { return TitleId; }
};
