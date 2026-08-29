#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "FastGame.h"

struct FASTGAME_API FFastGameConfig
{
	FString ApiBaseUrl = TEXT("http://api.localhost/api/v1");
	/**
	 * Active catalog game (storage NAME). Set once via Initialize Game / SetGameCode.
	 * Auth OTP / recovery / signup verify inject this as backend game_code — not auth Blueprint pins.
	 */
	FString GameCode;
	/**
	 * Target store / payment provider for this client build.
	 * myket | caffebazar | googleplay | steam | zarinpal | appstore
	 * Set via Initialize Game / SetStorePlatform. Empty shop Provider pins use this.
	 * Does NOT store auth identity — Enter alone persists identity.
	 */
	FString StorePlatform;
	/** Myket / Cafe Bazaar RSA public key (not Fast Game api_secret). Fetched from Editor after login; optional local override. */
	FString StorePublicKey;
	FString PendingPaymentSaveSlot = TEXT("FastGameShopPending");
	/** Saved under ProjectSaved/FastGame/. Empty string disables persistence. */
	FString AccessTokenSaveSlot = TEXT("FastGameAccessToken");
	/** ENTER identity cache (identity + channel). Empty disables persistence. */
	FString EnteredIdentitySaveSlot = TEXT("FastGameEnteredIdentity");
};

/** Lowercase provider id. Accepts cafebazaar / cafe_bazaar / bazaar → caffebazar. */
FASTGAME_API FString FastGameNormalizeProviderId(const FString& Provider);

/** Designer-facing store name (Cafe Bazaar, Myket, Google Play). */
FASTGAME_API FString FastGameStoreDisplayName(const FString& Provider);

/** Error when the store APK is not on the device. */
FASTGAME_API FString FastGameStoreNotInstalledMessage(const FString& Provider);

struct FASTGAME_API FFastGameCatalogEntry
{
	FString Id;
	FString GameId;
	FString Label;
	FString Description;
	FString ColyseusRoom;
	bool bAvailable = true;
	/** See docs/entity-locales.md — en/fa/ar name+description. */
	TSharedPtr<FJsonObject> Translations;
};

struct FASTGAME_API FFastGameMode
{
	FString Id;
	FString ModeId;
	FString Topology;
	FString WinKind;
	int32 MinPlayers = 0;
	int32 MaxPlayers = 0;
	FString Kind;
};

struct FASTGAME_API FFastGameMap
{
	FString Id;
	FString MapId;
	FString Label;
	TArray<FString> SupportedModes;
	bool bPurchasable = false;
	int32 Price = 0;
	TSharedPtr<FJsonObject> Translations;
};

struct FASTGAME_API FFastGameAssetPack
{
	FString Id;
	FString PackId;
	FString Label;
	int32 Revision = 0;
	FString Version;
	FString Url;
	FString Hash;
	/** mobile | pc | * — DOWNLOAD filter (A4). */
	TArray<FString> Quality;
	/** android | ios | windows | mac | web | * */
	TArray<FString> Platforms;
	/** BCP-47 tags or * — matched to preferred_language. */
	TArray<FString> Languages;
	/** content | locale | splash | upscale */
	FString Kind = TEXT("content");
};

struct FASTGAME_API FFastGameCatalogDetail : public FFastGameCatalogEntry
{
	TArray<FFastGameMode> Modes;
	TArray<FFastGameMap> Maps;
	TArray<FFastGameAssetPack> AssetPacks;
	/** New-user OTP gates from catalog auth_requirements. */
	bool bAuthVerifyPhone = false;
	bool bAuthVerifyEmail = false;
};

struct FASTGAME_API FFastGameCharacter
{
	FString Id;
	FString CharacterId;
	FString Label;
	/** player | npc | both */
	FString Role;
	FString BodyKind;
	TSharedPtr<FJsonObject> Stats;
	/**
	 * Entity locales: { "en": { "name", "description" }, "fa"?, "ar"? }.
	 * Events never use this — they have a single Name string. See docs/entity-locales.md.
	 */
	TSharedPtr<FJsonObject> Translations;
	int32 SortOrder = 0;
};

struct FASTGAME_API FFastGameShopLine
{
	FString GameCode;
	FString SkuKind;
	FString SkuId;
	FString Label;
	int32 Price = 0;
	bool bOwned = false;
	TSharedPtr<FJsonObject> Meta;
};

struct FASTGAME_API FFastGameLoadout
{
	FString UserId;
	FString GameCode;
	FString CharacterId;
	TMap<FString, FString> EquippedCosmetics;
	TMap<FString, FString> ModularParts;
	int32 Level = 0;
	int32 Xp = 0;
};

struct FASTGAME_API FFastGamePaymentInitiate
{
	FString Authority;
	FString PaymentUrl;
	FString PaymentToken;
	int32 Amount = 0;
	FString Provider;
	FString StoreProductId;
	FString OrderId;
};

struct FASTGAME_API FFastGameShopUnlock
{
	bool bOwned = false;
	bool bPending = false;
	bool bLocked = false;
	FString Mode;
	FString Provider;
	FString Authority;
	FString PaymentToken;
	FString PaymentUrl;
	FString StoreProductId;
	FString OrderId;
	int32 Amount = 0;
	FString Currency;
};

struct FASTGAME_API FFastGameUser
{
	FString Id;
	FString Email;
	FString Phone;
	bool bEmailVerified = false;
	bool bPhoneVerified = false;
	FString FullName;
	bool bIsActive = true;
	bool bIsSuperuser = false;
};

struct FASTGAME_API FFastGamePreparedSession
{
	FFastGameCatalogDetail Game;
	TSharedPtr<FJsonObject> MapRuntime;
	TSharedPtr<FJsonObject> Spawn;
	FString GameId;
	FString ModeId;
	FString MapId;
	FString ColyseusRoom;
};

/** POST /apps/games/realtime/seat — JoinMap ticket (prefer over GetGameServer). */
struct FASTGAME_API FFastGameSeatMint
{
	FString SeatToken;
	FString ExpiresAt;
	FString GameServerUrl;
	FString RoomName;
	FString GameId;
	FString MapId;
	FString ModeId;
};

struct FASTGAME_API FFastGameAdvertisementRequest
{
	FString GameId;
	FString Slot;
	FString MediaType;
	FString Format;
	TArray<FString> Tags;
	FString Locale;
	FString Country;
	FString Platform;
	FString Engine;
	/** Extensible capabilities object (e.g. mediaTypes). */
	TSharedPtr<FJsonObject> Capabilities;
};

struct FASTGAME_API FFastGameAdvertisement
{
	FString Id;
	FString CampaignId;
	FString MediaType;
	FString MediaUrl;
	int32 MediaWidth = 0;
	int32 MediaHeight = 0;
	bool bClickEnabled = false;
	FString ClickUrl;
	FString ImpressionTrackingUrl;
	FString ClickTrackingUrl;
	/** Text ads / extras — keys: title, body, background_url, background_color, … */
	TSharedPtr<FJsonObject> Meta;
	FString Title;
	FString Body;
	FString BackgroundUrl;
	FString BackgroundColor;
};

struct FASTGAME_API FFastGameAdvertisementEvent
{
	/** AdvertisementDisplayed | AdvertisementClicked | AdvertisementClosed */
	FString EventType;
	FString AdId;
	FString GameId;
	FString CampaignId;
	FString Timestamp;
	TSharedPtr<FJsonObject> Extras;
};
