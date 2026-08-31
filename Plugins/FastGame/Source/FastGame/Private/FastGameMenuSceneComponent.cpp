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

void UFastGameMenuSceneComponent::OpenLevelFromMap(const FFastGameBPMap& Map)
{
	if (Map.EngineScene.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[FastGame Menu] Cannot open level: engine_scene is not configured for map %s."),
			*Map.MapId);
		return;
	}

	OpenLevel(FName(*Map.EngineScene));
}
