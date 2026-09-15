#pragma once

#include "CoreMinimal.h"
#include "FastGameCharacterControllerComponent.h"
#include "FastGameMovementRuntimeComponent.generated.h"

/** Legacy G1 name — prefer Character Controller (V2). */
UCLASS(ClassGroup = (FastGame), meta = (BlueprintSpawnableComponent, DisplayName = "Movement Runtime (legacy)"))
class FASTGAME_API UFastGameMovementRuntimeComponent : public UFastGameCharacterControllerComponent
{
	GENERATED_BODY()
};
