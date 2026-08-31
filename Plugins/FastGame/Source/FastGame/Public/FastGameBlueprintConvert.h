#pragma once

#include "CoreMinimal.h"
#include "FastGameBlueprintTypes.h"
#include "FastGameTypes.h"

namespace FastGameBlueprintConvert
{
	FString JsonObjectToString(const TSharedPtr<FJsonObject>& Obj);
	TSharedPtr<FJsonObject> ParseJsonObject(const FString& JsonText);

	FFastGameBPCatalogEntry ToBP(const FFastGameCatalogEntry& In);
	FFastGameBPMode ToBP(const FFastGameMode& In);
	FFastGameBPMap ToBP(const FFastGameMap& In);
	FFastGameBPAssetPack ToBP(const FFastGameAssetPack& In);
	FFastGameBPCatalogDetail ToBP(const FFastGameCatalogDetail& In);
	FFastGameBPCharacter ToBP(const FFastGameCharacter& In);
	FFastGameBPShopLine ToBP(const FFastGameShopLine& In);
	FFastGameBPLoadout ToBP(const FFastGameLoadout& In);
	FFastGameBPPaymentInitiate ToBP(const FFastGamePaymentInitiate& In);
	FFastGameBPShopUnlock ToBP(const FFastGameShopUnlock& In);
	FFastGameBPPreparedSession ToBP(const FFastGamePreparedSession& In);
	FFastGameBPUser ToBP(const FFastGameUser& In);
	FFastGameBPAdvertisement ToBP(const FFastGameAdvertisement& In);
	FFastGameBPSeatMint ToBP(const FFastGameSeatMint& In);

	FFastGameShopLine FromBP(const FFastGameBPShopLine& In);

	TArray<FFastGameBPCatalogEntry> ToBPArray(const TArray<FFastGameCatalogEntry>& In);
	TArray<FFastGameBPCharacter> ToBPArray(const TArray<FFastGameCharacter>& In);
	TArray<FFastGameBPShopLine> ToBPArray(const TArray<FFastGameShopLine>& In);
	TArray<FFastGameBPAssetPack> ToBPArray(const TArray<FFastGameAssetPack>& In);

	FString StorePlatformToId(EFastGameStorePlatform Platform);
	EFastGameStorePlatform StorePlatformFromId(const FString& Provider);
	FString ProjectStageToWire(EFastGameProjectStage Stage);

	EFastGameShopProgress ClassifyShopProgress(bool bOwned, bool bPending, bool bOk, const FString& Message);
}
