#include "FastGameSceneFlowComponent.h"
#include "Kismet/GameplayStatics.h"

UFastGameSceneFlowComponent::UFastGameSceneFlowComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SceneName = FName(TEXT("MAP_0_SPLASH"));
	NextScene = FName(TEXT("MAP_1_LANGUAGE"));
}

void UFastGameSceneFlowComponent::CompleteScene()
{
	OnSceneComplete.Broadcast(SceneName);
	if (bAutoOpenNextOnComplete && !NextScene.IsNone())
	{
		OpenScene(NextScene);
	}
}

void UFastGameSceneFlowComponent::OpenNextScene()
{
	if (!NextScene.IsNone())
	{
		OpenScene(NextScene);
	}
}

void UFastGameSceneFlowComponent::OpenScene(FName LevelName)
{
	if (LevelName.IsNone())
	{
		return;
	}
	UGameplayStatics::OpenLevel(this, LevelName);
}
