#include "FastGameMenuSceneComponent.h"
#include "Kismet/GameplayStatics.h"

void UFastGameMenuSceneComponent::OpenLevel(FName LevelScene)
{
	const FName Target = LevelScene.IsNone() ? DefaultLevelScene : LevelScene;
	if (!Target.IsNone())
	{
		UGameplayStatics::OpenLevel(this, Target);
	}
}
