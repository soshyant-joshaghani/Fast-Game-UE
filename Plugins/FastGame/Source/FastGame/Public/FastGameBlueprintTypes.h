#pragma once

#include "CoreMinimal.h"
#include "FastGameBlueprintTypes.generated.h"

/** Blueprint mirror of catalog list entry. Translations as JSON string. */
USTRUCT(BlueprintType)
struct FASTGAME_API FFastGameBPCatalogEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString GameId;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString Label;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString Description;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString ColyseusRoom;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	bool bAvailable = true;

	/** Entity locales JSON — see docs/entity-locales.md. */
	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString TranslationsJson;
};

USTRUCT(BlueprintType)
struct FASTGAME_API FFastGameBPMode
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString ModeId;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString Topology;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString WinKind;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	int32 MinPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	int32 MaxPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString Kind;
};

USTRUCT(BlueprintType)
struct FASTGAME_API FFastGameBPMapRuntimeSettings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	TArray<FString> AbilityAllowlist;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	bool bChatEnabled = true;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	bool bEmojiEnabled = true;

	/** 0 = unlimited (typical for hubs). */
	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	int32 MaxPlayers = 0;
};

USTRUCT(BlueprintType)
struct FASTGAME_API FFastGameBPMap
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString MapId;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString Label;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString EngineScene;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString MapKind = TEXT("level");

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	TArray<FString> HubMapIds;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FFastGameBPMapRuntimeSettings RuntimeSettings;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	TArray<FString> SupportedModes;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	bool bPurchasable = false;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	int32 Price = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString TranslationsJson;
};

USTRUCT(BlueprintType)
struct FASTGAME_API FFastGameBPAssetPack
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString PackId;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString Label;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	int32 Revision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString Version;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString Url;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString Hash;

	/** mobile | pc | * — DOWNLOAD filter (A4). */
	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	TArray<FString> Quality;

	/** android | ios | windows | mac | web | * */
	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	TArray<FString> Platforms;

	/** BCP-47 tags or * — matched to preferred_language. */
	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	TArray<FString> Languages;

	/** content | locale | splash | upscale */
	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString Kind = TEXT("content");
};

USTRUCT(BlueprintType)
struct FASTGAME_API FFastGameBPCatalogDetail
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString GameId;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString Label;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString Description;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString ColyseusRoom;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	bool bAvailable = true;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString TranslationsJson;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	TArray<FFastGameBPMode> Modes;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	TArray<FFastGameBPMap> Maps;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	TArray<FFastGameBPAssetPack> AssetPacks;
};

USTRUCT(BlueprintType)
struct FASTGAME_API FFastGameBPCharacter
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString CharacterId;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString Label;

	/** player | npc | both */
	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString Role;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString BodyKind;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString StatsJson;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString TranslationsJson;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	int32 SortOrder = 0;
};

USTRUCT(BlueprintType)
struct FASTGAME_API FFastGameBPShopLine
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "FastGame")
	FString GameCode;

	UPROPERTY(BlueprintReadWrite, Category = "FastGame")
	FString SkuKind;

	UPROPERTY(BlueprintReadWrite, Category = "FastGame")
	FString SkuId;

	UPROPERTY(BlueprintReadWrite, Category = "FastGame")
	FString Label;

	UPROPERTY(BlueprintReadWrite, Category = "FastGame")
	int32 Price = 0;

	UPROPERTY(BlueprintReadWrite, Category = "FastGame")
	bool bOwned = false;

	UPROPERTY(BlueprintReadWrite, Category = "FastGame")
	FString MetaJson;
};

USTRUCT(BlueprintType)
struct FASTGAME_API FFastGameBPLoadout
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString UserId;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString GameCode;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString CharacterId;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	TMap<FString, FString> EquippedCosmetics;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	TMap<FString, FString> ModularParts;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	int32 Level = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	int32 Xp = 0;
};

USTRUCT(BlueprintType)
struct FASTGAME_API FFastGameBPPaymentInitiate
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString Authority;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString PaymentUrl;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString PaymentToken;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	int32 Amount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString Provider;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString StoreProductId;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString OrderId;
};

USTRUCT(BlueprintType)
struct FASTGAME_API FFastGameBPShopUnlock
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	bool bOwned = false;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	bool bPending = false;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	bool bLocked = false;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString Mode;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString Provider;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString Authority;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString PaymentToken;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString PaymentUrl;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString StoreProductId;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString OrderId;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	int32 Amount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString Currency;
};

USTRUCT(BlueprintType)
struct FASTGAME_API FFastGameBPUser
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString Email;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString Phone;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	bool bEmailVerified = false;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	bool bPhoneVerified = false;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString FullName;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	bool bIsActive = true;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	bool bIsSuperuser = false;
};

USTRUCT(BlueprintType)
struct FASTGAME_API FFastGameBPPreparedSession
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FFastGameBPCatalogDetail Game;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString MapRuntimeJson;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString SpawnJson;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString GameId;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString ModeId;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString MapId;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString ColyseusRoom;
};

/** Provider-opaque advertisement for Blueprint (media URL + click + optional text meta). */
USTRUCT(BlueprintType)
struct FASTGAME_API FFastGameBPAdvertisement
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FastGame|Ads")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame|Ads")
	FString CampaignId;

	/** image | gif | video | lottie | rive | text */
	UPROPERTY(BlueprintReadOnly, Category = "FastGame|Ads")
	FString MediaType;

	/** Primary media / asset URL (image, video, rive, …). Empty for pure text. */
	UPROPERTY(BlueprintReadOnly, Category = "FastGame|Ads")
	FString MediaUrl;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame|Ads")
	int32 MediaWidth = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame|Ads")
	int32 MediaHeight = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame|Ads")
	bool bClickEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame|Ads")
	FString ClickUrl;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame|Ads")
	FString ImpressionTrackingUrl;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame|Ads")
	FString ClickTrackingUrl;

	/** Text ad headline (from meta.title). */
	UPROPERTY(BlueprintReadOnly, Category = "FastGame|Ads")
	FString Title;

	/** Text ad body (from meta.body). */
	UPROPERTY(BlueprintReadOnly, Category = "FastGame|Ads")
	FString Body;

	/** Optional background image URL (from meta.background_url). */
	UPROPERTY(BlueprintReadOnly, Category = "FastGame|Ads")
	FString BackgroundUrl;

	/** Optional background color hex/rgba (from meta.background_color). */
	UPROPERTY(BlueprintReadOnly, Category = "FastGame|Ads")
	FString BackgroundColor;

	/** Full meta JSON for advanced use. */
	UPROPERTY(BlueprintReadOnly, Category = "FastGame|Ads")
	FString MetaJson;
};

/** Initialize Game store / payment platform. Empty shop Provider pins use this. */
UENUM(BlueprintType)
enum class EFastGameStorePlatform : uint8
{
	Unset UMETA(DisplayName = "Unset"),
	Myket UMETA(DisplayName = "Myket"),
	CafeBazaar UMETA(DisplayName = "Cafe Bazaar"),
	GooglePlay UMETA(DisplayName = "Google Play"),
	Steam UMETA(DisplayName = "Steam"),
	ZarinPal UMETA(DisplayName = "ZarinPal"),
	AppStore UMETA(DisplayName = "App Store"),
};

/** Client build stage — access token must match this stage on Initialize Client. */
UENUM(BlueprintType)
enum class EFastGameProjectStage : uint8
{
	Dev UMETA(DisplayName = "Dev"),
	Production UMETA(DisplayName = "Production"),
	EarlyAccess UMETA(DisplayName = "Early Access"),
};

/** How Enter / Login interpret Identity: Auto detects email vs phone. */
UENUM(BlueprintType)
enum class EFastGameIdentityChannel : uint8
{
	Auto UMETA(DisplayName = "Auto"),
	Email UMETA(DisplayName = "Email"),
	Phone UMETA(DisplayName = "Phone"),
};

/**
 * Internal ENTER route (LastEnterRoute). CompleteAccount is folded into the Signup
 * Blueprint pin but kept here so Register can call /complete for seeded users.
 */
UENUM(BlueprintType)
enum class EFastGameEnterRoute : uint8
{
	Login UMETA(DisplayName = "Enter Password"),
	CompleteAccount UMETA(DisplayName = "Complete Account"),
	VerifyId UMETA(DisplayName = "Verify"),
	Register UMETA(DisplayName = "Signup"),
	Failed UMETA(DisplayName = "Failed"),
};

/**
 * Designer Enter exec pins (ExpandEnumAsExecs). Seeded password_required fires Signup;
 * LastEnterRoute remains CompleteAccount for Register dispatch.
 */
UENUM(BlueprintType)
enum class EFastGameEnterPin : uint8
{
	EnterPassword UMETA(DisplayName = "Enter Password"),
	Verify UMETA(DisplayName = "Verify"),
	Signup UMETA(DisplayName = "Signup"),
	Failed UMETA(DisplayName = "Failed"),
};

/** Success | Failed exec pins for latent auth / simple shop nodes. */
UENUM(BlueprintType)
enum class EFastGameRequestOutcome : uint8
{
	Success UMETA(DisplayName = "Success"),
	Failed UMETA(DisplayName = "Failed"),
};

/**
 * Verify Auth Code exec pins after a successful OTP check.
 * Signup → Register (name + password); Assign New Password → Assign New Password node (password only).
 */
UENUM(BlueprintType)
enum class EFastGameVerifyAuthPin : uint8
{
	Signup UMETA(DisplayName = "Signup"),
	AssignNewPassword UMETA(DisplayName = "Assign New Password"),
	Failed UMETA(DisplayName = "Failed"),
};

/** Check Authentication exec pins (session gate). */
UENUM(BlueprintType)
enum class EFastGameAuthCheck : uint8
{
	Authenticated UMETA(DisplayName = "Authenticated"),
	NotAuthenticated UMETA(DisplayName = "Not Authenticated"),
	Failed UMETA(DisplayName = "Failed"),
};

/** Unified auth success — login, signup, or password recovery (wire scene flow). */
UENUM(BlueprintType)
enum class EFastGameAuthCompleteReason : uint8
{
	Login UMETA(DisplayName = "Login"),
	Signup UMETA(DisplayName = "Signup"),
	PasswordRecovery UMETA(DisplayName = "Password Recovery"),
	AlreadyAuthenticated UMETA(DisplayName = "Already Authenticated"),
};

/** Get Shop Sku Access exec pins. */
UENUM(BlueprintType)
enum class EFastGameShopAccessRoute : uint8
{
	Owned UMETA(DisplayName = "Owned"),
	Available UMETA(DisplayName = "Available"),
	Locked UMETA(DisplayName = "Locked"),
	Failed UMETA(DisplayName = "Failed"),
};

/** Shop Progress exec pins — Unlock Sku, return from payment, or app resume. */
UENUM(BlueprintType)
enum class EFastGameShopProgress : uint8
{
	Success UMETA(DisplayName = "Purchase Successful"),
	Pending UMETA(DisplayName = "Purchase Pending"),
	Failed UMETA(DisplayName = "Purchase Failed"),
	Cancelled UMETA(DisplayName = "Purchase Cancelled"),
	StoreMissing UMETA(DisplayName = "Store Missing"),
};

/** Travel Map exec pins (Flow action — offline open level vs online seat mint). */
UENUM(BlueprintType)
enum class EFastGameTravelMapPin : uint8
{
	Traveled UMETA(DisplayName = "Traveled"),
	Matchmaking UMETA(DisplayName = "Matchmaking"),
	WaitingHere UMETA(DisplayName = "Waiting Here"),
	Failed UMETA(DisplayName = "Failed"),
};

/** ON_QUEST listener exec pins (Flow listener). */
UENUM(BlueprintType)
enum class EFastGameQuestPin : uint8
{
	Complete UMETA(DisplayName = "Complete"),
	Failed UMETA(DisplayName = "Failed"),
	NotStartedYet UMETA(DisplayName = "Not Started Yet"),
};

/** JoinMap seat mint for Blueprint (Realtime.JoinMap step 1). */
USTRUCT(BlueprintType)
struct FASTGAME_API FFastGameBPSeatMint
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString SeatToken;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString ExpiresAt;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString GameServerUrl;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString RoomName;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString GameId;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString MapId;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame")
	FString ModeId;
};

// --- Dynamic multicast delegates (async Blueprint events) ---

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnFastGameLoginComplete, bool, bSuccess, int32, StatusCode, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFastGameAuthComplete, EFastGameAuthCompleteReason, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_SixParams(FOnFastGameSignupComplete, bool, bSuccess, int32, StatusCode, const FString&, UserId, const FString&, Email, const FString&, Phone, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnFastGameGetMeComplete, bool, bSuccess, int32, StatusCode, const FFastGameBPUser&, User, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFastGameSimpleComplete, bool, bSuccess, const FString&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnFastGameSteamStatus, bool, bSuccess, bool, bLinked, const FString&, SteamId, const FString&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnFastGameListGames, bool, bSuccess, const TArray<FFastGameBPCatalogEntry>&, Games, const FString&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnFastGameGetGame, bool, bSuccess, const FFastGameBPCatalogDetail&, Game, const FString&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnFastGameGetGameServer, bool, bSuccess, const FString&, Url, const FString&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnFastGameListCharacters, bool, bSuccess, const TArray<FFastGameBPCharacter>&, Characters, const FString&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnFastGamePrepareSession, bool, bSuccess, const FFastGameBPPreparedSession&, Session, const FString&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnFastGameJsonResult, bool, bSuccess, const FString&, JsonBody, const FString&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnFastGameCharacterConfigFetched, bool, bSuccess, const FString&, JsonBody, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnFastGameMapConfigFetched, bool, bSuccess, const FString&, JsonBody, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFastGameQuestPin, FName, QuestId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnFastGameLoadout, bool, bSuccess, const FFastGameBPLoadout&, Loadout, const FString&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnFastGameShopCatalog, bool, bSuccess, const TArray<FFastGameBPShopLine>&, Lines, const FString&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnFastGameShopSkuAccess, bool, bSuccess, bool, bLocked, bool, bOwned, const FString&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnFastGamePaymentInitiate, bool, bSuccess, const FFastGameBPPaymentInitiate&, Payment, const FString&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnFastGameUnlockSku, bool, bSuccess, bool, bOwned, const FFastGameBPShopUnlock&, Pending, const FString&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnFastGameShopProgress, EFastGameShopProgress, Progress, bool, bOwned, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnFastGamePaymentVerify, bool, bSuccess, bool, bPaymentSuccess, const FString&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnFastGameAdvertisement, bool, bSuccess, bool, bHasAd, const FFastGameBPAdvertisement&, Ad, const FString&, Error);
