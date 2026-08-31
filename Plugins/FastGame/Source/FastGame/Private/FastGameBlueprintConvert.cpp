#include "FastGameBlueprintConvert.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace FastGameBlueprintConvert
{
	FString JsonObjectToString(const TSharedPtr<FJsonObject>& Obj)
	{
		if (!Obj.IsValid())
		{
			return FString();
		}
		FString Out;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
		return Out;
	}

	TSharedPtr<FJsonObject> ParseJsonObject(const FString& JsonText)
	{
		TSharedPtr<FJsonValue> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid() || Root->Type != EJson::Object)
		{
			return nullptr;
		}
		return Root->AsObject();
	}

	FFastGameBPCatalogEntry ToBP(const FFastGameCatalogEntry& In)
	{
		FFastGameBPCatalogEntry Out;
		Out.Id = In.Id;
		Out.GameId = In.GameId;
		Out.Label = In.Label;
		Out.Description = In.Description;
		Out.ColyseusRoom = In.ColyseusRoom;
		Out.bAvailable = In.bAvailable;
		Out.TranslationsJson = JsonObjectToString(In.Translations);
		return Out;
	}

	FFastGameBPMode ToBP(const FFastGameMode& In)
	{
		FFastGameBPMode Out;
		Out.Id = In.Id;
		Out.ModeId = In.ModeId;
		Out.Topology = In.Topology;
		Out.WinKind = In.WinKind;
		Out.MinPlayers = In.MinPlayers;
		Out.MaxPlayers = In.MaxPlayers;
		Out.Kind = In.Kind;
		return Out;
	}

	FFastGameBPMap ToBP(const FFastGameMap& In)
	{
		FFastGameBPMap Out;
		Out.Id = In.Id;
		Out.MapId = In.MapId;
		Out.Label = In.Label;
		Out.EngineScene = In.EngineScene;
		Out.MapKind = In.MapKind;
		Out.HubMapIds = In.HubMapIds;
		Out.RuntimeSettings.AbilityAllowlist = In.RuntimeSettings.AbilityAllowlist;
		Out.RuntimeSettings.bChatEnabled = In.RuntimeSettings.bChatEnabled;
		Out.RuntimeSettings.bEmojiEnabled = In.RuntimeSettings.bEmojiEnabled;
		Out.RuntimeSettings.MaxPlayers = In.RuntimeSettings.MaxPlayers;
		Out.SupportedModes = In.SupportedModes;
		Out.bPurchasable = In.bPurchasable;
		Out.Price = In.Price;
		Out.TranslationsJson = JsonObjectToString(In.Translations);
		return Out;
	}

	FFastGameBPAssetPack ToBP(const FFastGameAssetPack& In)
	{
		FFastGameBPAssetPack Out;
		Out.Id = In.Id;
		Out.PackId = In.PackId;
		Out.Label = In.Label;
		Out.Revision = In.Revision;
		Out.Version = In.Version;
		Out.Url = In.Url;
		Out.Hash = In.Hash;
		Out.Quality = In.Quality;
		Out.Platforms = In.Platforms;
		Out.Languages = In.Languages;
		Out.Kind = In.Kind;
		return Out;
	}

	FFastGameBPCatalogDetail ToBP(const FFastGameCatalogDetail& In)
	{
		FFastGameBPCatalogDetail Out;
		Out.Id = In.Id;
		Out.GameId = In.GameId;
		Out.Label = In.Label;
		Out.Description = In.Description;
		Out.ColyseusRoom = In.ColyseusRoom;
		Out.bAvailable = In.bAvailable;
		Out.TranslationsJson = JsonObjectToString(In.Translations);
		for (const FFastGameMode& M : In.Modes)
		{
			Out.Modes.Add(ToBP(M));
		}
		for (const FFastGameMap& M : In.Maps)
		{
			Out.Maps.Add(ToBP(M));
		}
		for (const FFastGameAssetPack& P : In.AssetPacks)
		{
			Out.AssetPacks.Add(ToBP(P));
		}
		return Out;
	}

	FFastGameBPCharacter ToBP(const FFastGameCharacter& In)
	{
		FFastGameBPCharacter Out;
		Out.Id = In.Id;
		Out.CharacterId = In.CharacterId;
		Out.Label = In.Label;
		Out.Role = In.Role;
		Out.BodyKind = In.BodyKind;
		Out.StatsJson = JsonObjectToString(In.Stats);
		Out.TranslationsJson = JsonObjectToString(In.Translations);
		Out.SortOrder = In.SortOrder;
		return Out;
	}

	FFastGameBPShopLine ToBP(const FFastGameShopLine& In)
	{
		FFastGameBPShopLine Out;
		Out.GameCode = In.GameCode;
		Out.SkuKind = In.SkuKind;
		Out.SkuId = In.SkuId;
		Out.Label = In.Label;
		Out.Price = In.Price;
		Out.bOwned = In.bOwned;
		Out.MetaJson = JsonObjectToString(In.Meta);
		return Out;
	}

	FFastGameBPLoadout ToBP(const FFastGameLoadout& In)
	{
		FFastGameBPLoadout Out;
		Out.UserId = In.UserId;
		Out.GameCode = In.GameCode;
		Out.CharacterId = In.CharacterId;
		Out.EquippedCosmetics = In.EquippedCosmetics;
		Out.ModularParts = In.ModularParts;
		Out.Level = In.Level;
		Out.Xp = In.Xp;
		return Out;
	}

	FFastGameBPPaymentInitiate ToBP(const FFastGamePaymentInitiate& In)
	{
		FFastGameBPPaymentInitiate Out;
		Out.Authority = In.Authority;
		Out.PaymentUrl = In.PaymentUrl;
		Out.PaymentToken = In.PaymentToken;
		Out.Amount = In.Amount;
		Out.Provider = In.Provider;
		Out.StoreProductId = In.StoreProductId;
		Out.OrderId = In.OrderId;
		return Out;
	}

	FFastGameBPShopUnlock ToBP(const FFastGameShopUnlock& In)
	{
		FFastGameBPShopUnlock Out;
		Out.bOwned = In.bOwned;
		Out.bPending = In.bPending;
		Out.bLocked = In.bLocked;
		Out.Mode = In.Mode;
		Out.Provider = In.Provider;
		Out.Authority = In.Authority;
		Out.PaymentToken = In.PaymentToken;
		Out.PaymentUrl = In.PaymentUrl;
		Out.StoreProductId = In.StoreProductId;
		Out.OrderId = In.OrderId;
		Out.Amount = In.Amount;
		Out.Currency = In.Currency;
		return Out;
	}

	FFastGameBPPreparedSession ToBP(const FFastGamePreparedSession& In)
	{
		FFastGameBPPreparedSession Out;
		Out.Game = ToBP(In.Game);
		Out.MapRuntimeJson = JsonObjectToString(In.MapRuntime);
		Out.SpawnJson = JsonObjectToString(In.Spawn);
		Out.GameId = In.GameId;
		Out.ModeId = In.ModeId;
		Out.MapId = In.MapId;
		Out.ColyseusRoom = In.ColyseusRoom;
		return Out;
	}

	FFastGameBPUser ToBP(const FFastGameUser& In)
	{
		FFastGameBPUser Out;
		Out.Id = In.Id;
		Out.Email = In.Email;
		Out.Phone = In.Phone;
		Out.bEmailVerified = In.bEmailVerified;
		Out.bPhoneVerified = In.bPhoneVerified;
		Out.FullName = In.FullName;
		Out.bIsActive = In.bIsActive;
		Out.bIsSuperuser = In.bIsSuperuser;
		return Out;
	}

	FFastGameBPAdvertisement ToBP(const FFastGameAdvertisement& In)
	{
		FFastGameBPAdvertisement Out;
		Out.Id = In.Id;
		Out.CampaignId = In.CampaignId;
		Out.MediaType = In.MediaType;
		Out.MediaUrl = In.MediaUrl;
		Out.MediaWidth = In.MediaWidth;
		Out.MediaHeight = In.MediaHeight;
		Out.bClickEnabled = In.bClickEnabled;
		Out.ClickUrl = In.ClickUrl;
		Out.ImpressionTrackingUrl = In.ImpressionTrackingUrl;
		Out.ClickTrackingUrl = In.ClickTrackingUrl;
		Out.Title = In.Title;
		Out.Body = In.Body;
		Out.BackgroundUrl = In.BackgroundUrl;
		Out.BackgroundColor = In.BackgroundColor;
		Out.MetaJson = JsonObjectToString(In.Meta);
		return Out;
	}

	FFastGameShopLine FromBP(const FFastGameBPShopLine& In)
	{
		FFastGameShopLine Out;
		Out.GameCode = In.GameCode;
		Out.SkuKind = In.SkuKind;
		Out.SkuId = In.SkuId;
		Out.Label = In.Label;
		Out.Price = In.Price;
		Out.bOwned = In.bOwned;
		// Meta stays empty on buy path — C++ shop uses Sku fields primarily
		return Out;
	}

	TArray<FFastGameBPCatalogEntry> ToBPArray(const TArray<FFastGameCatalogEntry>& In)
	{
		TArray<FFastGameBPCatalogEntry> Out;
		Out.Reserve(In.Num());
		for (const FFastGameCatalogEntry& E : In)
		{
			Out.Add(ToBP(E));
		}
		return Out;
	}

	TArray<FFastGameBPCharacter> ToBPArray(const TArray<FFastGameCharacter>& In)
	{
		TArray<FFastGameBPCharacter> Out;
		Out.Reserve(In.Num());
		for (const FFastGameCharacter& C : In)
		{
			Out.Add(ToBP(C));
		}
		return Out;
	}

	TArray<FFastGameBPShopLine> ToBPArray(const TArray<FFastGameShopLine>& In)
	{
		TArray<FFastGameBPShopLine> Out;
		Out.Reserve(In.Num());
		for (const FFastGameShopLine& L : In)
		{
			Out.Add(ToBP(L));
		}
		return Out;
	}

	TArray<FFastGameBPAssetPack> ToBPArray(const TArray<FFastGameAssetPack>& In)
	{
		TArray<FFastGameBPAssetPack> Out;
		Out.Reserve(In.Num());
		for (const FFastGameAssetPack& P : In)
		{
			Out.Add(ToBP(P));
		}
		return Out;
	}

	FString StorePlatformToId(EFastGameStorePlatform Platform)
	{
		switch (Platform)
		{
		case EFastGameStorePlatform::Myket: return TEXT("myket");
		case EFastGameStorePlatform::CafeBazaar: return TEXT("caffebazar");
		case EFastGameStorePlatform::GooglePlay: return TEXT("googleplay");
		case EFastGameStorePlatform::Steam: return TEXT("steam");
		case EFastGameStorePlatform::ZarinPal: return TEXT("zarinpal");
		case EFastGameStorePlatform::AppStore: return TEXT("appstore");
		default: return TEXT("");
		}
	}

	EFastGameStorePlatform StorePlatformFromId(const FString& Provider)
	{
		const FString Id = FastGameNormalizeProviderId(Provider);
		if (Id == TEXT("myket")) return EFastGameStorePlatform::Myket;
		if (Id == TEXT("caffebazar")) return EFastGameStorePlatform::CafeBazaar;
		if (Id == TEXT("googleplay")) return EFastGameStorePlatform::GooglePlay;
		if (Id == TEXT("steam")) return EFastGameStorePlatform::Steam;
		if (Id == TEXT("zarinpal")) return EFastGameStorePlatform::ZarinPal;
		if (Id == TEXT("appstore")) return EFastGameStorePlatform::AppStore;
		return EFastGameStorePlatform::Unset;
	}

	EFastGameShopProgress ClassifyShopProgress(bool bOwned, bool bPending, bool bOk, const FString& Message)
	{
		const FString Lower = Message.ToLower();
		if (Lower.Contains(TEXT("is not installed"))
			|| Lower.Contains(TEXT("store missing"))
			|| Lower.Contains(TEXT("plugin not loaded")))
		{
			return EFastGameShopProgress::StoreMissing;
		}
		if (Lower.Contains(TEXT("cancel")))
		{
			return EFastGameShopProgress::Cancelled;
		}
		if (bOwned)
		{
			return EFastGameShopProgress::Success;
		}
		if (bPending)
		{
			return EFastGameShopProgress::Pending;
		}
		if (!bOk)
		{
			return EFastGameShopProgress::Failed;
		}
		return EFastGameShopProgress::Success;
	}
}
