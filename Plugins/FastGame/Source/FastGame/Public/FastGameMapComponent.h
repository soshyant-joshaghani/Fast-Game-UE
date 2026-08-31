#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/LatentActionManager.h"
#include "FastGameBlueprintTypes.h"
#include "FastGameMapComponent.generated.h"

/**
 * Binds map + mode NAMEs on a level prefab — GetMapConfig, Travel Map (Flow action),
 * and ON_QUEST listener events.
 */
UCLASS(ClassGroup = (FastGame), meta = (BlueprintSpawnableComponent))
class FASTGAME_API UFastGameMapComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Locale-free catalog NAME for this map (e.g. MAP_LEVEL_SAMPLE). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Map")
	FName MapId;

	/** Active mode NAME (e.g. solo, pvp). Empty → solo/offline travel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Map")
	FName ModeId;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Map")
	FOnFastGameMapConfigFetched OnMapConfigFetched;

	/** ON_QUEST listener — Complete exec pin. */
	UPROPERTY(BlueprintAssignable, Category = "FastGame|Map|Quest")
	FOnFastGameQuestPin OnQuestComplete;

	/** ON_QUEST listener — Failed exec pin. */
	UPROPERTY(BlueprintAssignable, Category = "FastGame|Map|Quest")
	FOnFastGameQuestPin OnQuestFailed;

	/** ON_QUEST listener — Not Started Yet exec pin. */
	UPROPERTY(BlueprintAssignable, Category = "FastGame|Map|Quest")
	FOnFastGameQuestPin OnQuestNotStartedYet;

	/** Empty GameCode → Initialize Game GameCode. Uses component MapId. */
	UFUNCTION(BlueprintCallable, Category = "FastGame|Map", meta = (Latent, LatentInfo = "LatentInfo", DisplayName = "Get Map Config", CPP_Default_GameCode = ""))
	void GetMapConfig(
		const FString& GameCode,
		FLatentActionInfo LatentInfo,
		bool& bSuccess,
		int32& StatusCode,
		FString& Message,
		FString& JsonBody);

	/**
	 * Travel Map Flow action. Empty TargetMapId → component MapId.
	 * Pins: Traveled | Matchmaking | Waiting Here | Failed.
	 * Online: mints seat (Matchmaking) — join sibling Colyseus, then open level.
	 * Solo: opens target engine_scene (Traveled).
	 * Same-map travel → Waiting Here (optional seat mint when online).
	 */
	UFUNCTION(BlueprintCallable, Category = "FastGame|Map", meta = (Latent, LatentInfo = "LatentInfo",
		ExpandEnumAsExecs = "Pin", DisplayName = "Travel Map", CPP_Default_GameCode = "", CPP_Default_TargetMapId = ""))
	void TravelMap(
		const FString& GameCode,
		FName TargetMapId,
		FLatentActionInfo LatentInfo,
		UPARAM(DisplayName = "Pin") EFastGameTravelMapPin& Pin,
		int32& StatusCode,
		FString& Message,
		FFastGameBPSeatMint& Seat);

	/** Flow driver / kernel hook — fire ON_QUEST Complete pin. */
	UFUNCTION(BlueprintCallable, Category = "FastGame|Map|Quest", meta = (DisplayName = "Notify Quest Complete"))
	void NotifyQuestComplete(FName QuestId);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Map|Quest", meta = (DisplayName = "Notify Quest Failed"))
	void NotifyQuestFailed(FName QuestId);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Map|Quest", meta = (DisplayName = "Notify Quest Not Started Yet"))
	void NotifyQuestNotStartedYet(FName QuestId);

	UFUNCTION(BlueprintPure, Category = "FastGame|Map", meta = (DisplayName = "Get Map Id"))
	FName GetMapId() const { return MapId; }

	UFUNCTION(BlueprintPure, Category = "FastGame|Map", meta = (DisplayName = "Get Mode Id"))
	FName GetModeId() const { return ModeId; }
};
