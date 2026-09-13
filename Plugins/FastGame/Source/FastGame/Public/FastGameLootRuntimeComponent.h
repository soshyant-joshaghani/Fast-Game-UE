#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/LatentActionManager.h"
#include "FastGameBlueprintTypes.h"
#include "FastGameLootRuntimeComponent.generated.h"

class UFastGameMapComponent;
class UFastGameParamRuntimeComponent;

/** G2 LootRuntime — GetLootTable + validated Open Loot (server roll). */
UCLASS(ClassGroup = (FastGame), meta = (BlueprintSpawnableComponent))
class FASTGAME_API UFastGameLootRuntimeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Loot")
	TObjectPtr<UFastGameMapComponent> Map;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Loot")
	TObjectPtr<UFastGameParamRuntimeComponent> Params;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Loot")
	FName OpenParamName = TEXT("IsOpen");

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Loot")
	FOnFastGameJsonResult OnLootTableFetched;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Loot")
	FOnFastGameJsonResult OnLootOpened;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Loot")
	FOnFastGameDirectorMessage OnLootFailed;

	UFUNCTION(BlueprintCallable, Category = "FastGame|Loot", meta = (Latent, LatentInfo = "LatentInfo", DisplayName = "Get Loot Table", CPP_Default_GameCode = ""))
	void GetLootTable(
		const FString& GameCode,
		FName LootTableId,
		FLatentActionInfo LatentInfo,
		bool& bSuccess,
		int32& StatusCode,
		FString& Message,
		FString& JsonBody);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Loot", meta = (Latent, LatentInfo = "LatentInfo", DisplayName = "Open Loot", CPP_Default_GameCode = "", CPP_Default_ModeId = "", CPP_Default_PlacementId = "", CPP_Default_LootTableId = ""))
	void OpenLoot(
		const FString& GameCode,
		FName PickupId,
		FName PlacementId,
		FName LootTableId,
		FName ModeId,
		FLatentActionInfo LatentInfo,
		bool& bSuccess,
		int32& StatusCode,
		FString& Message,
		FString& JsonBody);

protected:
	virtual void BeginPlay() override;

	void ResolveModules();
};
