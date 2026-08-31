#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/LatentActionManager.h"
#include "FastGameBlueprintTypes.h"
#include "FastGameCharacterComponent.generated.h"

/**
 * Binds a catalog character NAME on a prefab — fetches progressive tip via GetCharacter.
 * Wire local mesh/anim from project assets; config comes from Fast-Game.
 */
UCLASS(ClassGroup = (FastGame), meta = (BlueprintSpawnableComponent))
class FASTGAME_API UFastGameCharacterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Locale-free catalog NAME (e.g. PLAYER_SAMPLE). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FastGame|Character")
	FName CharacterId;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Character")
	FOnFastGameCharacterConfigFetched OnCharacterFetched;

	/** Empty GameCode → Initialize Game GameCode. */
	UFUNCTION(BlueprintCallable, Category = "FastGame|Character", meta = (Latent, LatentInfo = "LatentInfo", DisplayName = "Fetch Character", CPP_Default_GameCode = ""))
	void FetchCharacter(
		const FString& GameCode,
		FLatentActionInfo LatentInfo,
		bool& bSuccess,
		int32& StatusCode,
		FString& Message,
		FString& JsonBody);

	UFUNCTION(BlueprintPure, Category = "FastGame|Character", meta = (DisplayName = "Get Character Id"))
	FName GetCharacterId() const { return CharacterId; }
};
