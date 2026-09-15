#pragma once

#include "CoreMinimal.h"
#include "FastGameCameraControllerComponent.h"
#include "FastGameCameraRuntimeComponent.generated.h"

/** Legacy G1 name — prefer Camera Controller (V2). */
UCLASS(ClassGroup = (FastGame), meta = (BlueprintSpawnableComponent, DisplayName = "Camera Runtime (legacy)"))
class FASTGAME_API UFastGameCameraRuntimeComponent : public UFastGameCameraControllerComponent
{
	GENERATED_BODY()
};
