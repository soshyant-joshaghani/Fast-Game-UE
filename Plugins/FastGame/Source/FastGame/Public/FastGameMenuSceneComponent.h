#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FastGameBlueprintTypes.h"
#include "FastGameMenuSceneComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFastGameMenuPageChanged);

/**
 * MENU hub page switching — mirrors Unity FastGameMenuSceneBehaviour.
 * Wire UMG visibility from OnMenuPageChanged or call Show* from widget buttons.
 */
UCLASS(ClassGroup = (FastGame), meta = (BlueprintSpawnableComponent))
class FASTGAME_API UFastGameMenuSceneComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "FastGame|Menu")
	FOnFastGameMenuPageChanged OnShowMenu;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Menu")
	FOnFastGameMenuPageChanged OnShowShop;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Menu")
	FOnFastGameMenuPageChanged OnShowCollectibles;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Menu")
	FOnFastGameMenuPageChanged OnShowAchievements;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Menu")
	FOnFastGameMenuPageChanged OnShowTitles;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Menu")
	FOnFastGameMenuPageChanged OnShowAvatars;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Menu")
	FOnFastGameMenuPageChanged OnInspectAchievement;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Menu")
	FOnFastGameMenuPageChanged OnInspectTitle;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Menu")
	FOnFastGameMenuPageChanged OnInspectAvatar;

	UFUNCTION(BlueprintCallable, Category = "FastGame|Menu")
	void ShowMenu() { OnShowMenu.Broadcast(); }

	UFUNCTION(BlueprintCallable, Category = "FastGame|Menu")
	void ShowShop() { OnShowShop.Broadcast(); }

	UFUNCTION(BlueprintCallable, Category = "FastGame|Menu")
	void ShowCollectibles() { OnShowCollectibles.Broadcast(); }

	UFUNCTION(BlueprintCallable, Category = "FastGame|Menu")
	void ShowAchievements() { OnShowAchievements.Broadcast(); }

	UFUNCTION(BlueprintCallable, Category = "FastGame|Menu")
	void ShowTitles() { OnShowTitles.Broadcast(); }

	UFUNCTION(BlueprintCallable, Category = "FastGame|Menu")
	void ShowAvatars() { OnShowAvatars.Broadcast(); }

	UFUNCTION(BlueprintCallable, Category = "FastGame|Menu")
	void InspectAchievement() { OnInspectAchievement.Broadcast(); }

	UFUNCTION(BlueprintCallable, Category = "FastGame|Menu")
	void InspectTitle() { OnInspectTitle.Broadcast(); }

	UFUNCTION(BlueprintCallable, Category = "FastGame|Menu")
	void InspectAvatar() { OnInspectAvatar.Broadcast(); }

	UFUNCTION(BlueprintCallable, Category = "FastGame|Menu")
	void OpenLevel(FName LevelScene);

	/** Loads catalog engine_scene for a map row (Play solo parity with Unity menu). */
	UFUNCTION(BlueprintCallable, Category = "FastGame|Menu")
	void OpenLevelFromMap(const FFastGameBPMap& Map);
};
