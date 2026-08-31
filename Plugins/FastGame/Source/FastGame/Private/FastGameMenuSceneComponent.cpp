#include "FastGameMenuSceneComponent.h"
#include "Kismet/GameplayStatics.h"

void UFastGameMenuSceneComponent::OpenLevel(FName LevelScene)
{
	if (LevelScene.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[FastGame Menu] Cannot open level: engine_scene is not configured."));
		return;
	}

	UGameplayStatics::OpenLevel(this, LevelScene);
}
