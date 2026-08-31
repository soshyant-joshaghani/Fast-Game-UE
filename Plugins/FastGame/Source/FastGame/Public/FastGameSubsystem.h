#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/LatentActionManager.h"
#include "FastGameBlueprintTypes.h"
#include "FastGameSubsystem.generated.h"

class FFastGameClient;

/**
 * Blueprint facade over FFastGameClient.
 * Initialize Game (1x build + OS) then Initialize Client (Nx network). Then Enter / Login / etc.
 */
UCLASS(DisplayName = "Fast Game")
class FASTGAME_API UFastGameSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- Lifecycle ---

	/**
	 * One-time build config + OS store check (Myket / Cafe Bazaar / Play APK).
	 * Persists GameCode / StorePlatform even before Initialize Client. Does not wipe token or Enter identity.
	 */
	UFUNCTION(BlueprintCallable, Category = "FastGame", meta = (DisplayName = "Initialize Game", CPP_Default_StorePlatform = "Unset"))
	void InitializeGame(
		const FString& GameCode,
		EFastGameStorePlatform StorePlatform,
		bool& bSuccess,
		FString& Message);

	/**
	 * Network / reconnect only (1x or Nx). ApiBaseUrl, HTTP client, restore token.
	 * Registers project stage + client access token with the server.
	 */
	UFUNCTION(BlueprintCallable, Category = "FastGame", meta = (DisplayName = "Initialize Client", CPP_Default_ProjectStage = "Dev", CPP_Default_ClientAccessToken = "fg-dev-game"))
	void InitializeClient(
		const FString& ApiBaseUrl,
		EFastGameProjectStage ProjectStage,
		const FString& ClientAccessToken,
		bool& bSuccess,
		FString& Message);

	/** Optional Blueprint override. Leave empty — SDK fetches Cafe Bazaar / Myket RSA from Editor after login (never JWT / api_secret). */
	UFUNCTION(BlueprintCallable, Category = "FastGame", meta = (DisplayName = "Set Store Public Key"))
	void SetStorePublicKey(const FString& PublicKey);

	UFUNCTION(BlueprintPure, Category = "FastGame", meta = (DisplayName = "Get Store Public Key"))
	FString GetStorePublicKey() const;

	/** Active catalog game for auth OTP / recovery (also set via Initialize Game). */
	UFUNCTION(BlueprintCallable, Category = "FastGame", meta = (DisplayName = "Set Game Code"))
	void SetGameCode(const FString& GameCode);

	UFUNCTION(BlueprintPure, Category = "FastGame", meta = (DisplayName = "Get Game Code"))
	FString GetGameCode() const;

	/** Target store for this APK (myket / caffebazar / googleplay / steam / zarinpal). Set via Initialize Game. Empty shop Provider uses this. */
	UFUNCTION(BlueprintCallable, Category = "FastGame", meta = (DisplayName = "Set Store Platform"))
	void SetStorePlatform(EFastGameStorePlatform StorePlatform);

	UFUNCTION(BlueprintPure, Category = "FastGame", meta = (DisplayName = "Get Store Platform"))
	EFastGameStorePlatform GetStorePlatform() const;

	UFUNCTION(BlueprintPure, Category = "FastGame", meta = (DisplayName = "Get Store Platform Id"))
	FString GetStorePlatformId() const;

	UFUNCTION(BlueprintPure, Category = "FastGame")
	bool IsInitialized() const;

	/** Native C++ client (FastGameStore Purchase Or Restore). */
	TSharedPtr<FFastGameClient> GetClient() const;

	UFUNCTION(BlueprintPure, Category = "FastGame|Auth", meta = (DisplayName = "Is Authenticated"))
	bool IsAuthenticated() const;

	UFUNCTION(BlueprintPure, Category = "FastGame")
	bool IsLoggedIn() const;

	/**
	 * Latent auth gate. Pins: Authenticated | Not Authenticated | Failed.
	 * Clears a bad token on Not Authenticated so Is Authenticated becomes false.
	 */
	UFUNCTION(BlueprintCallable, Category = "FastGame|Auth", meta = (Latent, LatentInfo = "LatentInfo",
		ExpandEnumAsExecs = "Check", DisplayName = "Check Authentication"))
	void CheckAuthentication(
		FLatentActionInfo LatentInfo,
		UPARAM(DisplayName = "Check") EFastGameAuthCheck& Check,
		int32& StatusCode,
		FString& Message);

	UFUNCTION(BlueprintPure, Category = "FastGame")
	FString GetAccessToken() const;

	UFUNCTION(BlueprintCallable, Category = "FastGame")
	void SetAccessToken(const FString& Token);

	UFUNCTION(BlueprintCallable, Category = "FastGame")
	void Logout();

	/** Clear access token, ENTER-stored identity, and pending-payment cache. Use on login page as a dev tool. */
	UFUNCTION(BlueprintCallable, Category = "FastGame|Auth")
	void ClearLocalCache();

	/** Result of the most recent Login call (also true after successful Signup auto-login). */
	UFUNCTION(BlueprintPure, Category = "FastGame|Auth")
	bool GetLastLoginSucceeded() const { return bLastLoginSucceeded; }

	/** Result of the most recent Signup call. */
	UFUNCTION(BlueprintPure, Category = "FastGame|Auth")
	bool GetLastSignupSucceeded() const { return bLastSignupSucceeded; }

	// --- Auth (latent: scenario exec pins; no redundant bSuccess) ---

	/**
	 * ENTER contract: probe Identity (POST /base/login/enter). No widgets — only route exec pins.
	 * Channel: Auto (detect), Email, or Phone.
	 * Pins: Enter Password | Verify | Signup | Failed.
	 * Seeded password_required fires Signup (LastEnterRoute stays CompleteAccount for Register).
	 * Forgot: from Enter Password call Send Auth Code (recovery OTP) — no Begin Forgot node.
	 */
	UFUNCTION(BlueprintCallable, Category = "FastGame|Auth", meta = (Latent, LatentInfo = "LatentInfo",
		ExpandEnumAsExecs = "Pin", DisplayName = "Enter"))
	void Enter(
		const FString& Identity,
		EFastGameIdentityChannel Channel,
		FLatentActionInfo LatentInfo,
		UPARAM(DisplayName = "Pin") EFastGameEnterPin& Pin,
		FString& Message,
		FString& OutIdentity,
		FString& OutEmail,
		FString& OutPhone,
		bool& bOutEmail,
		bool& bOutPhone);

	/**
	 * Login with email or phone identity. Empty Identity → ENTER store.
	 * Pins: Success | Failed.
	 */
	UFUNCTION(BlueprintCallable, Category = "FastGame|Auth", meta = (Latent, LatentInfo = "LatentInfo",
		ExpandEnumAsExecs = "Outcome", DisplayName = "Login"))
	void Login(
		const FString& Identity,
		const FString& Password,
		EFastGameIdentityChannel Channel,
		FLatentActionInfo LatentInfo,
		UPARAM(DisplayName = "Outcome") EFastGameRequestOutcome& Outcome,
		int32& StatusCode,
		FString& Message);

	/** Clear ENTER-stored identity (Saved/FastGame/FastGameEnteredIdentity.txt). */
	UFUNCTION(BlueprintCallable, Category = "FastGame|Auth", meta = (DisplayName = "Clear Entered Identity"))
	void ClearEnteredIdentity();

	/** ENTER-stored identity (empty if none). Loads from disk if needed. */
	UFUNCTION(BlueprintPure, Category = "FastGame|Auth", meta = (DisplayName = "Get Entered Identity"))
	FString GetEnteredIdentity() const;

	UFUNCTION(BlueprintPure, Category = "FastGame|Auth", meta = (DisplayName = "Has Entered Identity"))
	bool HasEnteredIdentity() const;

	UFUNCTION(BlueprintPure, Category = "FastGame|Auth", meta = (DisplayName = "Get Entered Channel"))
	EFastGameIdentityChannel GetEnteredChannel() const;

	/** Last Enter route — drives Send/Verify Auth Code and Register dispatch. */
	UFUNCTION(BlueprintPure, Category = "FastGame|Auth", meta = (DisplayName = "Get Last Enter Route"))
	EFastGameEnterRoute GetLastEnterRoute() const { return LastEnterRoute; }

	/**
	 * Signup credentials (+ auto-login). Email and/or Phone empty → ENTER store.
	 * If LastEnterRoute is CompleteAccount (seeded), calls /complete; otherwise /signup.
	 * Pins: Success | Failed.
	 */
	UFUNCTION(BlueprintCallable, Category = "FastGame|Auth", meta = (Latent, LatentInfo = "LatentInfo",
		ExpandEnumAsExecs = "Outcome", DisplayName = "Register"))
	void Signup(
		const FString& Email,
		const FString& Phone,
		const FString& Password,
		const FString& PasswordConfirm,
		const FString& FullName,
		FLatentActionInfo LatentInfo,
		UPARAM(DisplayName = "Outcome") EFastGameRequestOutcome& Outcome,
		int32& StatusCode,
		FString& UserId,
		FString& OutEmail,
		FString& OutPhone,
		FString& Message);

	/**
	 * Assign new password after forgot OTP (password + confirm, no name).
	 * Empty Identity → ENTER store. Wire from Verify Auth Code → Assign New Password.
	 * Pins: Success | Failed.
	 */
	UFUNCTION(BlueprintCallable, Category = "FastGame|Auth", meta = (Latent, LatentInfo = "LatentInfo",
		ExpandEnumAsExecs = "Outcome", DisplayName = "Assign New Password"))
	void ConfirmPasswordRecovery(
		const FString& Identity,
		const FString& NewPassword,
		const FString& NewPasswordConfirm,
		FLatentActionInfo LatentInfo,
		UPARAM(DisplayName = "Outcome") EFastGameRequestOutcome& Outcome,
		int32& StatusCode,
		FString& Message);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Auth", meta = (DisplayName = "Back To Enter ID"))
	void BackToEnterId();

	/** Call from OTP widget OnShown — auto-sends OTP once per visit when enabled. */
	UFUNCTION(BlueprintCallable, Category = "FastGame|Auth", meta = (DisplayName = "Notify OTP Page Shown"))
	void NotifyOtpPageShown(bool bAutoSend = true);

	/** Enter Password → forgot → OTP recovery (sets forgot flow flag). */
	UFUNCTION(BlueprintCallable, Category = "FastGame|Auth", meta = (DisplayName = "Begin Forgot Password"))
	void BeginForgotPassword();

	/**
	 * Shared OTP send. Empty Identity → ENTER store.
	 * Enter → Verify → signup OTP; Enter Password → recovery OTP (forgot); otherwise fails.
	 * Pins: Success | Failed.
	 */
	UFUNCTION(BlueprintCallable, Category = "FastGame|Auth", meta = (Latent, LatentInfo = "LatentInfo",
		ExpandEnumAsExecs = "Outcome", DisplayName = "Send Auth Code"))
	void SendAuthCode(
		const FString& Identity,
		FLatentActionInfo LatentInfo,
		UPARAM(DisplayName = "Outcome") EFastGameRequestOutcome& Outcome,
		int32& StatusCode,
		FString& Message);

	/**
	 * Shared OTP verify. Empty Identity → ENTER store.
	 * Pins: Signup | Assign New Password | Failed.
	 * Signup → Register (name + password); Assign New Password → Assign New Password node.
	 */
	UFUNCTION(BlueprintCallable, Category = "FastGame|Auth", meta = (Latent, LatentInfo = "LatentInfo",
		ExpandEnumAsExecs = "Pin", DisplayName = "Verify Auth Code"))
	void VerifyAuthCode(
		const FString& Identity,
		const FString& Code,
		FLatentActionInfo LatentInfo,
		UPARAM(DisplayName = "Pin") EFastGameVerifyAuthPin& Pin,
		int32& StatusCode,
		FString& Message);

	/** Classify a contact string as Email, Phone, or Unknown (for UI hints). */
	UFUNCTION(BlueprintPure, Category = "FastGame|Auth")
	static bool IsEmailIdentity(const FString& Identity);

	UFUNCTION(BlueprintPure, Category = "FastGame|Auth")
	static bool IsPhoneIdentity(const FString& Identity);

	/** GetMe. Pins: Success | Failed. User / CurrentUser hold the profile. */
	UFUNCTION(BlueprintCallable, Category = "FastGame|Auth", meta = (Latent, LatentInfo = "LatentInfo",
		ExpandEnumAsExecs = "Outcome", DisplayName = "Get Me"))
	void GetMe(
		FLatentActionInfo LatentInfo,
		UPARAM(DisplayName = "Outcome") EFastGameRequestOutcome& Outcome,
		int32& StatusCode,
		FFastGameBPUser& User,
		FString& Message);

	/** PATCH /base/login/me — display name only. Pins: Success | Failed. */
	UFUNCTION(BlueprintCallable, Category = "FastGame|Auth", meta = (Latent, LatentInfo = "LatentInfo",
		ExpandEnumAsExecs = "Outcome", DisplayName = "Update Full Name"))
	void UpdateFullName(
		const FString& FullName,
		FLatentActionInfo LatentInfo,
		UPARAM(DisplayName = "Outcome") EFastGameRequestOutcome& Outcome,
		int32& StatusCode,
		FFastGameBPUser& User,
		FString& Message);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Auth", meta = (Latent, LatentInfo = "LatentInfo",
		ExpandEnumAsExecs = "Outcome"))
	void LinkSteamWithTicket(
		const FString& Ticket,
		const FString& Identity,
		FLatentActionInfo LatentInfo,
		UPARAM(DisplayName = "Outcome") EFastGameRequestOutcome& Outcome,
		int32& StatusCode,
		FString& Message,
		bool& bLinked,
		FString& SteamId);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Auth", meta = (Latent, LatentInfo = "LatentInfo",
		ExpandEnumAsExecs = "Outcome"))
	void GetSteamStatus(
		FLatentActionInfo LatentInfo,
		UPARAM(DisplayName = "Outcome") EFastGameRequestOutcome& Outcome,
		int32& StatusCode,
		FString& Message,
		bool& bLinked,
		FString& SteamId);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Auth", meta = (Latent, LatentInfo = "LatentInfo",
		ExpandEnumAsExecs = "Outcome"))
	void UnlinkSteam(
		FLatentActionInfo LatentInfo,
		UPARAM(DisplayName = "Outcome") EFastGameRequestOutcome& Outcome,
		int32& StatusCode,
		FString& Message);

	// --- Catalog ---

	/** Lang: BCP-47 tag for resolved labels (e.g. fa). Expand I18n: include full translations maps. */
	UFUNCTION(BlueprintCallable, Category = "FastGame|Catalog", meta = (Latent, LatentInfo = "LatentInfo", AdvancedDisplay = "Lang,bExpandI18n", CPP_Default_Lang = "", CPP_Default_bExpandI18n = "false"))
	void ListGames(
		bool bAvailableOnly,
		const FString& Lang,
		bool bExpandI18n,
		FLatentActionInfo LatentInfo,
		bool& bSuccess,
		int32& StatusCode,
		FString& Message,
		TArray<FFastGameBPCatalogEntry>& Games);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Catalog", meta = (Latent, LatentInfo = "LatentInfo", AdvancedDisplay = "Lang,bExpandI18n", CPP_Default_Lang = "", CPP_Default_bExpandI18n = "false"))
	void GetGame(
		const FString& GameId,
		const FString& Lang,
		bool bExpandI18n,
		FLatentActionInfo LatentInfo,
		bool& bSuccess,
		int32& StatusCode,
		FString& Message,
		FFastGameBPCatalogDetail& Game);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Catalog", meta = (Latent, LatentInfo = "LatentInfo"))
	void GetGameServer(
		FLatentActionInfo LatentInfo,
		bool& bSuccess,
		int32& StatusCode,
		FString& Message,
		FString& Url);

	// --- Content (tip façade preferred for players) ---

	/** SPLASH GetBootstrap — published tip metadata. */
	UFUNCTION(BlueprintCallable, Category = "FastGame|Content", meta = (Latent, LatentInfo = "LatentInfo", DisplayName = "Get Bootstrap"))
	void GetBootstrap(
		const FString& GameCode,
		FLatentActionInfo LatentInfo,
		bool& bSuccess,
		int32& StatusCode,
		FString& Message,
		FString& JsonBody);

	/** Player GetGameConfig — tip payload (404 if unpublished). */
	UFUNCTION(BlueprintCallable, Category = "FastGame|Content", meta = (Latent, LatentInfo = "LatentInfo", DisplayName = "Get Game Config"))
	void GetGameConfig(
		const FString& GameCode,
		FLatentActionInfo LatentInfo,
		bool& bSuccess,
		int32& StatusCode,
		FString& Message,
		FString& JsonBody);

	/** Player GetMapConfig — tip map payload (404 if unpublished). */
	UFUNCTION(BlueprintCallable, Category = "FastGame|Content", meta = (Latent, LatentInfo = "LatentInfo", DisplayName = "Get Map Config"))
	void GetMapConfig(
		const FString& GameCode,
		const FString& MapId,
		FLatentActionInfo LatentInfo,
		bool& bSuccess,
		int32& StatusCode,
		FString& Message,
		FString& JsonBody);

	/** Progressive GetCharacter — tip character payload (404 if unpublished). */
	UFUNCTION(BlueprintCallable, Category = "FastGame|Content", meta = (Latent, LatentInfo = "LatentInfo", DisplayName = "Get Character"))
	void GetCharacter(
		const FString& GameCode,
		const FString& CharacterId,
		FLatentActionInfo LatentInfo,
		bool& bSuccess,
		int32& StatusCode,
		FString& Message,
		FString& JsonBody);

	/** @deprecated Prefer Get Bootstrap / Get Game Config for players. */
	UFUNCTION(BlueprintCallable, Category = "FastGame|Content", meta = (Latent, LatentInfo = "LatentInfo", AdvancedDisplay = "Lang,bExpandI18n", CPP_Default_Lang = "", CPP_Default_bExpandI18n = "false"))
	void ListCharacters(
		const FString& GameId,
		const FString& Role,
		const FString& Lang,
		bool bExpandI18n,
		FLatentActionInfo LatentInfo,
		bool& bSuccess,
		int32& StatusCode,
		FString& Message,
		TArray<FFastGameBPCharacter>& Characters);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Content", meta = (Latent, LatentInfo = "LatentInfo", AdvancedDisplay = "Lang,bExpandI18n", CPP_Default_Lang = "", CPP_Default_bExpandI18n = "false"))
	void PrepareSession(
		const FString& GameId,
		const FString& ModeId,
		const FString& MapId,
		const FString& Lang,
		bool bExpandI18n,
		FLatentActionInfo LatentInfo,
		bool& bSuccess,
		int32& StatusCode,
		FString& Message,
		FFastGameBPPreparedSession& Session);

	/** @deprecated Prefer Get Map Config for players. */
	UFUNCTION(BlueprintCallable, Category = "FastGame|Content", meta = (Latent, LatentInfo = "LatentInfo", AdvancedDisplay = "Lang,bExpandI18n", CPP_Default_Lang = "", CPP_Default_bExpandI18n = "false"))
	void GetMapRuntime(
		const FString& GameId,
		const FString& MapId,
		const FString& Lang,
		bool bExpandI18n,
		FLatentActionInfo LatentInfo,
		bool& bSuccess,
		int32& StatusCode,
		FString& Message,
		FString& JsonBody);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Content", meta = (Latent, LatentInfo = "LatentInfo", AdvancedDisplay = "Lang,bExpandI18n", CPP_Default_Lang = "", CPP_Default_bExpandI18n = "false"))
	void ResolveSpawn(
		const FString& GameId,
		const FString& MapId,
		const FString& ModeId,
		const FString& Lang,
		bool bExpandI18n,
		FLatentActionInfo LatentInfo,
		bool& bSuccess,
		int32& StatusCode,
		FString& Message,
		FString& JsonBody);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Content", meta = (Latent, LatentInfo = "LatentInfo"))
	void GetLoadout(
		const FString& GameId,
		FLatentActionInfo LatentInfo,
		bool& bSuccess,
		int32& StatusCode,
		FString& Message,
		FFastGameBPLoadout& Loadout);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Content", meta = (Latent, LatentInfo = "LatentInfo"))
	void SetLoadout(
		const FString& GameId,
		const FString& CharacterId,
		const TMap<FString, FString>& Cosmetics,
		const TMap<FString, FString>& ModularParts,
		FLatentActionInfo LatentInfo,
		bool& bSuccess,
		int32& StatusCode,
		FString& Message,
		FFastGameBPLoadout& Loadout);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Content", meta = (Latent, LatentInfo = "LatentInfo"))
	void ClaimPickup(
		const FString& GameId,
		const FString& MapId,
		const FString& PickupId,
		const FString& PlacementId,
		FLatentActionInfo LatentInfo,
		bool& bSuccess,
		int32& StatusCode,
		FString& Message,
		FString& JsonBody);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Content", meta = (Latent, LatentInfo = "LatentInfo"))
	void ClaimEvent(
		const FString& GameId,
		const FString& EventId,
		FLatentActionInfo LatentInfo,
		bool& bSuccess,
		int32& StatusCode,
		FString& Message,
		FString& JsonBody);

	// --- Shop ---

	UFUNCTION(BlueprintPure, Category = "FastGame|Shop")
	bool HasPendingPayment() const;

	/** Empty GameIdFilter → Initialize Game GameCode (same as Enter empty Identity). Pins: Success | Failed. */
	UFUNCTION(BlueprintCallable, Category = "FastGame|Shop", meta = (Latent, LatentInfo = "LatentInfo",
		ExpandEnumAsExecs = "Outcome", AdvancedDisplay = "Lang,bExpandI18n",
		CPP_Default_GameIdFilter = "", CPP_Default_Lang = "", CPP_Default_bExpandI18n = "false",
		DisplayName = "Get Shop Catalog"))
	void GetShopCatalog(
		const FString& GameIdFilter,
		const FString& Lang,
		bool bExpandI18n,
		FLatentActionInfo LatentInfo,
		UPARAM(DisplayName = "Outcome") EFastGameRequestOutcome& Outcome,
		int32& StatusCode,
		FString& Message,
		TArray<FFastGameBPShopLine>& Lines);

	/** Content lock + player purchase state. Empty GameCode → Initialize Game.
	 * Pins: Owned | Available | Locked | Failed.
	 * Android store: also queries native inventory (no purchase UI). If the store already owns
	 * the SKU, completes Unlock so Fast Game ownership matches. ZarinPal/Steam: Fast Game only. */
	UFUNCTION(BlueprintCallable, Category = "FastGame|Shop", meta = (Latent, LatentInfo = "LatentInfo",
		ExpandEnumAsExecs = "Access", CPP_Default_GameCode = "", DisplayName = "Get Shop Sku Access"))
	void GetShopSkuAccess(
		const FString& GameCode,
		const FString& SkuKind,
		const FString& SkuId,
		FLatentActionInfo LatentInfo,
		UPARAM(DisplayName = "Access") EFastGameShopAccessRoute& Access,
		int32& StatusCode,
		FString& Message);

	UFUNCTION(BlueprintPure, Category = "FastGame|Shop")
	bool IsShopLineLocked(const FFastGameBPShopLine& Line) const;

	UFUNCTION(BlueprintPure, Category = "FastGame|Shop")
	bool IsShopLineOwned(const FFastGameBPShopLine& Line) const;

	UFUNCTION(BlueprintCallable, Category = "FastGame|Shop", meta = (Latent, LatentInfo = "LatentInfo",
		ExpandEnumAsExecs = "Outcome", CPP_Default_GameCode = "", DisplayName = "Claim Free"))
	void ClaimFree(
		const FString& GameCode,
		const FString& SkuKind,
		const FString& SkuId,
		FLatentActionInfo LatentInfo,
		UPARAM(DisplayName = "Outcome") EFastGameRequestOutcome& Outcome,
		int32& StatusCode,
		FString& Message);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Shop", meta = (Latent, LatentInfo = "LatentInfo",
		ExpandEnumAsExecs = "Outcome", CPP_Default_GameCode = "", DisplayName = "Redeem Code"))
	void RedeemCode(
		const FString& GameCode,
		const FString& Code,
		FLatentActionInfo LatentInfo,
		UPARAM(DisplayName = "Outcome") EFastGameRequestOutcome& Outcome,
		int32& StatusCode,
		FString& Message);

	/**
	 * One designer purchase flow. Empty GameCode → Initialize Game.
	 * Pins: Purchase Successful | Pending | Failed | Cancelled | Store Missing.
	 */
	UFUNCTION(BlueprintCallable, Category = "FastGame|Shop", meta = (Latent, LatentInfo = "LatentInfo",
		ExpandEnumAsExecs = "Progress", DisplayName = "Unlock Sku",
		CPP_Default_GameCode = "", CPP_Default_CallbackUrl = "", CPP_Default_DiscountCode = ""))
	void UnlockSku(
		const FString& GameCode,
		const FString& SkuKind,
		const FString& SkuId,
		const FString& CallbackUrl,
		const FString& DiscountCode,
		FLatentActionInfo LatentInfo,
		UPARAM(DisplayName = "Progress") EFastGameShopProgress& Progress,
		int32& StatusCode,
		FString& Message,
		FFastGameBPShopUnlock& Pending);

	/** After ZarinPal return / Steam overlay / or a store token you already have.
	 * Pins: same Shop Progress scenario pins. */
	UFUNCTION(BlueprintCallable, Category = "FastGame|Shop", meta = (Latent, LatentInfo = "LatentInfo",
		ExpandEnumAsExecs = "Progress", DisplayName = "Complete Unlock"))
	void CompleteUnlock(
		const FString& PurchaseToken,
		FLatentActionInfo LatentInfo,
		UPARAM(DisplayName = "Progress") EFastGameShopProgress& Progress,
		int32& StatusCode,
		FString& Message);

	/**
	 * Shop return / progress (like Enter pins). Place on Event Application Has Reactivated
	 * after a payment, or after Unlock Sku.
	 * Pins: Purchase Successful | Purchase Pending | Purchase Failed | Purchase Cancelled | Store Missing.
	 */
	UFUNCTION(BlueprintCallable, Category = "FastGame|Shop", meta = (Latent, LatentInfo = "LatentInfo",
		ExpandEnumAsExecs = "Progress", DisplayName = "Shop Progress"))
	void ShopProgress(
		FLatentActionInfo LatentInfo,
		UPARAM(DisplayName = "Progress") EFastGameShopProgress& Progress,
		FString& Message);

	UFUNCTION(BlueprintPure, Category = "FastGame|Shop", meta = (DisplayName = "Get Last Shop Progress"))
	EFastGameShopProgress GetLastShopProgress() const { return LastShopProgress; }

	UFUNCTION(BlueprintCallable, Category = "FastGame|Shop")
	void ClearPendingPayment();

	// --- Ads ---

	/**
	 * Request a provider-opaque advertisement. bHasAd=false on HTTP 204 (no fill).
	 * Pass MediaType to prefer a type (image|gif|video|lottie|rive|text); leave empty for any.
	 */
	UFUNCTION(BlueprintCallable, Category = "FastGame|Ads", meta = (Latent, LatentInfo = "LatentInfo", AdvancedDisplay = "Format,Tags,Locale,Country,Platform,Engine"))
	void GetAdvertisement(
		const FString& GameId,
		const FString& Slot,
		const FString& MediaType,
		const FString& Format,
		const TArray<FString>& Tags,
		const FString& Locale,
		const FString& Country,
		const FString& Platform,
		const FString& Engine,
		FLatentActionInfo LatentInfo,
		bool& bSuccess,
		int32& StatusCode,
		FString& Message,
		bool& bHasAd,
		FFastGameBPAdvertisement& Ad);

	/** Image ad — MediaUrl/ImageUrl ready for UMG download / brush. */
	UFUNCTION(BlueprintCallable, Category = "FastGame|Ads", meta = (Latent, LatentInfo = "LatentInfo", AdvancedDisplay = "Format,Tags,Locale,Country,Platform,Engine", DisplayName = "Get Image Ad"))
	void GetImageAd(
		const FString& GameId,
		const FString& Slot,
		const FString& Format,
		const TArray<FString>& Tags,
		const FString& Locale,
		const FString& Country,
		const FString& Platform,
		const FString& Engine,
		FLatentActionInfo LatentInfo,
		bool& bSuccess,
		int32& StatusCode,
		FString& Message,
		bool& bHasAd,
		FString& ImageUrl,
		FString& ClickUrl,
		int32& Width,
		int32& Height,
		FFastGameBPAdvertisement& Ad);

	/** Video ad — VideoUrl for Media Player. */
	UFUNCTION(BlueprintCallable, Category = "FastGame|Ads", meta = (Latent, LatentInfo = "LatentInfo", AdvancedDisplay = "Format,Tags,Locale,Country,Platform,Engine", DisplayName = "Get Video Ad"))
	void GetVideoAd(
		const FString& GameId,
		const FString& Slot,
		const FString& Format,
		const TArray<FString>& Tags,
		const FString& Locale,
		const FString& Country,
		const FString& Platform,
		const FString& Engine,
		FLatentActionInfo LatentInfo,
		bool& bSuccess,
		int32& StatusCode,
		FString& Message,
		bool& bHasAd,
		FString& VideoUrl,
		FString& ClickUrl,
		FFastGameBPAdvertisement& Ad);

	/** Gif ad. */
	UFUNCTION(BlueprintCallable, Category = "FastGame|Ads", meta = (Latent, LatentInfo = "LatentInfo", AdvancedDisplay = "Format,Tags,Locale,Country,Platform,Engine", DisplayName = "Get Gif Ad"))
	void GetGifAd(
		const FString& GameId,
		const FString& Slot,
		const FString& Format,
		const TArray<FString>& Tags,
		const FString& Locale,
		const FString& Country,
		const FString& Platform,
		const FString& Engine,
		FLatentActionInfo LatentInfo,
		bool& bSuccess,
		int32& StatusCode,
		FString& Message,
		bool& bHasAd,
		FString& MediaUrl,
		FString& ClickUrl,
		FFastGameBPAdvertisement& Ad);

	/** Lottie ad — MediaUrl points at the Lottie JSON/file. */
	UFUNCTION(BlueprintCallable, Category = "FastGame|Ads", meta = (Latent, LatentInfo = "LatentInfo", AdvancedDisplay = "Format,Tags,Locale,Country,Platform,Engine", DisplayName = "Get Lottie Ad"))
	void GetLottieAd(
		const FString& GameId,
		const FString& Slot,
		const FString& Format,
		const TArray<FString>& Tags,
		const FString& Locale,
		const FString& Country,
		const FString& Platform,
		const FString& Engine,
		FLatentActionInfo LatentInfo,
		bool& bSuccess,
		int32& StatusCode,
		FString& Message,
		bool& bHasAd,
		FString& MediaUrl,
		FString& ClickUrl,
		FFastGameBPAdvertisement& Ad);

	/** Rive ad — MediaUrl points at the .riv asset. */
	UFUNCTION(BlueprintCallable, Category = "FastGame|Ads", meta = (Latent, LatentInfo = "LatentInfo", AdvancedDisplay = "Format,Tags,Locale,Country,Platform,Engine", DisplayName = "Get Rive Ad"))
	void GetRiveAd(
		const FString& GameId,
		const FString& Slot,
		const FString& Format,
		const TArray<FString>& Tags,
		const FString& Locale,
		const FString& Country,
		const FString& Platform,
		const FString& Engine,
		FLatentActionInfo LatentInfo,
		bool& bSuccess,
		int32& StatusCode,
		FString& Message,
		bool& bHasAd,
		FString& MediaUrl,
		FString& ClickUrl,
		FFastGameBPAdvertisement& Ad);

	/**
	 * Text ad — Title/Body + optional BackgroundUrl / BackgroundColor.
	 * Creative meta keys: title, body, background_url, background_color.
	 */
	UFUNCTION(BlueprintCallable, Category = "FastGame|Ads", meta = (Latent, LatentInfo = "LatentInfo", AdvancedDisplay = "Format,Tags,Locale,Country,Platform,Engine", DisplayName = "Get Text Ad"))
	void GetTextAd(
		const FString& GameId,
		const FString& Slot,
		const FString& Format,
		const TArray<FString>& Tags,
		const FString& Locale,
		const FString& Country,
		const FString& Platform,
		const FString& Engine,
		FLatentActionInfo LatentInfo,
		bool& bSuccess,
		int32& StatusCode,
		FString& Message,
		bool& bHasAd,
		FString& Title,
		FString& Body,
		FString& BackgroundUrl,
		FString& BackgroundColor,
		FString& ClickUrl,
		FFastGameBPAdvertisement& Ad);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Ads", meta = (Latent, LatentInfo = "LatentInfo"))
	void TrackAdEvent(
		const FString& EventType,
		const FString& AdId,
		const FString& GameId,
		const FString& CampaignId,
		FLatentActionInfo LatentInfo,
		bool& bSuccess,
		int32& StatusCode,
		FString& Message);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Ads", meta = (Latent, LatentInfo = "LatentInfo", DisplayName = "Track Ad Displayed"))
	void TrackAdDisplayed(
		const FFastGameBPAdvertisement& Ad,
		const FString& GameId,
		FLatentActionInfo LatentInfo,
		bool& bSuccess,
		int32& StatusCode,
		FString& Message);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Ads", meta = (Latent, LatentInfo = "LatentInfo", DisplayName = "Track Ad Clicked"))
	void TrackAdClicked(
		const FFastGameBPAdvertisement& Ad,
		const FString& GameId,
		FLatentActionInfo LatentInfo,
		bool& bSuccess,
		int32& StatusCode,
		FString& Message);

	UFUNCTION(BlueprintCallable, Category = "FastGame|Ads", meta = (Latent, LatentInfo = "LatentInfo", DisplayName = "Track Ad Closed"))
	void TrackAdClosed(
		const FFastGameBPAdvertisement& Ad,
		const FString& GameId,
		FLatentActionInfo LatentInfo,
		bool& bSuccess,
		int32& StatusCode,
		FString& Message);

	UFUNCTION(BlueprintPure, Category = "FastGame|Ads", meta = (DisplayName = "Is Image Ad"))
	bool IsImageAd(const FFastGameBPAdvertisement& Ad) const;

	UFUNCTION(BlueprintPure, Category = "FastGame|Ads", meta = (DisplayName = "Is Video Ad"))
	bool IsVideoAd(const FFastGameBPAdvertisement& Ad) const;

	UFUNCTION(BlueprintPure, Category = "FastGame|Ads", meta = (DisplayName = "Is Gif Ad"))
	bool IsGifAd(const FFastGameBPAdvertisement& Ad) const;

	UFUNCTION(BlueprintPure, Category = "FastGame|Ads", meta = (DisplayName = "Is Lottie Ad"))
	bool IsLottieAd(const FFastGameBPAdvertisement& Ad) const;

	UFUNCTION(BlueprintPure, Category = "FastGame|Ads", meta = (DisplayName = "Is Rive Ad"))
	bool IsRiveAd(const FFastGameBPAdvertisement& Ad) const;

	UFUNCTION(BlueprintPure, Category = "FastGame|Ads", meta = (DisplayName = "Is Text Ad"))
	bool IsTextAd(const FFastGameBPAdvertisement& Ad) const;

	/** Break Ad into common UMG pins (same fields as the struct). */
	UFUNCTION(BlueprintPure, Category = "FastGame|Ads", meta = (DisplayName = "Break Advertisement"))
	void BreakAdvertisement(
		const FFastGameBPAdvertisement& Ad,
		FString& Id,
		FString& CampaignId,
		FString& MediaType,
		FString& MediaUrl,
		int32& Width,
		int32& Height,
		bool& bClickEnabled,
		FString& ClickUrl,
		FString& Title,
		FString& Body,
		FString& BackgroundUrl,
		FString& BackgroundColor);

	// --- Assets ---

	UFUNCTION(BlueprintPure, Category = "FastGame|Assets", meta = (DisplayName = "List Packs From Game Config Json"))
	TArray<FFastGameBPAssetPack> ListPacksFromGameConfigJson(const FString& GameConfigJson) const;

	UFUNCTION(BlueprintPure, Category = "FastGame|Assets", meta = (DisplayName = "Filter Packs For Download"))
	TArray<FFastGameBPAssetPack> FilterPacksForDownload(
		const TArray<FFastGameBPAssetPack>& Packs,
		const FString& PreferredLanguage,
		bool bSkipSplashPacks = true) const;

	UFUNCTION(BlueprintPure, Category = "FastGame|Assets")
	TArray<FFastGameBPAssetPack> ListPacksFromGameDetail(const FFastGameBPCatalogDetail& Detail) const;

	// --- Language / platform (DOWNLOAD) ---

	UFUNCTION(BlueprintCallable, Category = "FastGame|Language", meta = (DisplayName = "Set Preferred Language"))
	void SetPreferredLanguage(const FString& Language);

	UFUNCTION(BlueprintPure, Category = "FastGame|Language", meta = (DisplayName = "Get Preferred Language"))
	FString GetPreferredLanguage() const;

	UFUNCTION(BlueprintPure, Category = "FastGame|Platform", meta = (DisplayName = "Get Runtime Os"))
	FString GetRuntimeOs() const;

	UFUNCTION(BlueprintPure, Category = "FastGame|Platform", meta = (DisplayName = "Get Quality Class"))
	FString GetQualityClass() const;

	// --- Delegates ---

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Auth")
	FOnFastGameLoginComplete OnLoginComplete;

	/** Fires once when the player has a session (login, signup, or password recovery). Wire BP_2_AUTH → DOWNLOAD. */
	UPROPERTY(BlueprintAssignable, Category = "FastGame|Auth")
	FOnFastGameAuthComplete OnAuthComplete;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Auth")
	FOnFastGameSimpleComplete OnBackToEnterId;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Auth")
	FOnFastGameSignupComplete OnSignupComplete;

	/** Most recent Login result — bind UI to this or read via Get Last Login Succeeded. */
	UPROPERTY(BlueprintReadOnly, Category = "FastGame|Auth")
	bool bLastLoginSucceeded = false;

	/** Most recent Signup result — bind UI to this or read via Get Last Signup Succeeded. */
	UPROPERTY(BlueprintReadOnly, Category = "FastGame|Auth")
	bool bLastSignupSucceeded = false;

	/** Route from the most recent Enter (drives Send/Verify Auth Code). */
	UPROPERTY(BlueprintReadOnly, Category = "FastGame|Auth")
	EFastGameEnterRoute LastEnterRoute = EFastGameEnterRoute::Failed;

	UPROPERTY(BlueprintReadOnly, Category = "FastGame|Auth")
	bool bForgotPasswordFlow = false;

	/** Cached profile from the last successful GetMe — bind Text / Image widgets to fields. */
	UPROPERTY(BlueprintReadOnly, Category = "FastGame|Auth")
	FFastGameBPUser CurrentUser;

	/** HTTP status from the last HTTP request (0 = local/network). */
	UPROPERTY(BlueprintReadOnly, Category = "FastGame|Auth")
	int32 LastAuthStatusCode = 0;

	/** Message from the last HTTP request (API detail or local error). */
	UPROPERTY(BlueprintReadOnly, Category = "FastGame|Auth")
	FString LastAuthMessage;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Auth")
	FOnFastGameGetMeComplete OnGetMeComplete;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Auth")
	FOnFastGameSteamStatus OnSteamLinkComplete;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Auth")
	FOnFastGameSteamStatus OnSteamStatus;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Auth")
	FOnFastGameSimpleComplete OnUnlinkSteamComplete;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Catalog")
	FOnFastGameListGames OnListGamesComplete;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Catalog")
	FOnFastGameGetGame OnGetGameComplete;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Catalog")
	FOnFastGameGetGameServer OnGetGameServerComplete;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Content")
	FOnFastGameJsonResult OnGetBootstrapComplete;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Content")
	FOnFastGameJsonResult OnGetGameConfigComplete;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Content")
	FOnFastGameJsonResult OnGetMapConfigComplete;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Content")
	FOnFastGameCharacterConfigFetched OnGetCharacterComplete;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Content")
	FOnFastGameListCharacters OnListCharactersComplete;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Content")
	FOnFastGamePrepareSession OnPrepareSessionComplete;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Content")
	FOnFastGameJsonResult OnMapRuntimeComplete;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Content")
	FOnFastGameJsonResult OnResolveSpawnComplete;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Content")
	FOnFastGameLoadout OnGetLoadoutComplete;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Content")
	FOnFastGameLoadout OnSetLoadoutComplete;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Content")
	FOnFastGameJsonResult OnClaimPickupComplete;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Content")
	FOnFastGameJsonResult OnClaimEventComplete;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Shop")
	FOnFastGameShopCatalog OnShopCatalogComplete;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Shop")
	FOnFastGameShopSkuAccess OnShopSkuAccessComplete;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Shop")
	FOnFastGameSimpleComplete OnClaimFreeComplete;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Shop")
	FOnFastGameUnlockSku OnUnlockSkuComplete;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Shop")
	FOnFastGameShopProgress OnShopProgress;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Shop")
	FOnFastGamePaymentVerify OnCompleteUnlockComplete;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Ads")
	FOnFastGameAdvertisement OnGetAdvertisementComplete;

	UPROPERTY(BlueprintAssignable, Category = "FastGame|Ads")
	FOnFastGameSimpleComplete OnTrackAdEventComplete;

private:
	bool EnsureClient(FString& OutError) const;
	void BroadcastAuthComplete(EFastGameAuthCompleteReason Reason);
	bool EnsureStoreSetup(FString& OutMessage) const;
	void SyncNativeStorePublicKey();
	void EnsureStoreVerifyKey(TFunction<void(bool, FString)>&& OnDone);
	void SetShopProgress(EFastGameShopProgress Progress, bool bOwned, const FString& Message);
	void HandleAppReactivated();
	void RunShopProgress(TFunction<void(EFastGameShopProgress, bool, FString)>&& OnDone);
	/** Marshal a callback to the game thread; no-ops if WeakThis is stale or Generation mismatches. */
	static void DispatchToGameThread(
		TWeakObjectPtr<UFastGameSubsystem> WeakThis,
		int32 Generation,
		TFunction<void(UFastGameSubsystem*)>&& Lambda);

	friend struct FFastGameAdsBlueprintHelper;

	void ApplyPersistedGameConfig();

	TSharedPtr<FFastGameClient> Client;
	FString StorePublicKey;
	FString PersistedGameCode;
	FString PersistedStorePlatformId;
	EFastGameProjectStage PersistedProjectStage = EFastGameProjectStage::Dev;
	FString PersistedClientAccessToken = TEXT("fg-dev-game");
	/** Bumped on InitializeClient / Deinitialize so orphaned HTTP callbacks are ignored. */
	int32 ClientGeneration = 0;
	/** Store SKUs already queried this session (Access must not reopen Cafe Bazaar every tick). */
	TSet<FString> NativeInventoryQueriedSkus;
	EFastGameShopProgress LastShopProgress = EFastGameShopProgress::Failed;
	bool bShopUnlockInFlight = false;
	FDelegateHandle AppReactivatedHandle;
	FDelegateHandle AppForegroundHandle;
	bool bOtpAutoSentThisVisit = false;
};

/** Internal Blueprint ads helpers (friend of UFastGameSubsystem). */
struct FFastGameAdsBlueprintHelper
{
	static void RequestAd(
		UFastGameSubsystem* Self,
		const FString& GameId,
		const FString& Slot,
		const FString& MediaType,
		const FString& Format,
		const TArray<FString>& Tags,
		const FString& Locale,
		const FString& Country,
		const FString& Platform,
		const FString& Engine,
		FLatentActionInfo LatentInfo,
		bool& bSuccess,
		int32& StatusCode,
		FString& Message,
		bool& bHasAd,
		FFastGameBPAdvertisement& Ad,
		TFunction<void(class FFastGameRequestLatentAction*)> BindExtraOutputs);

	static void Track(
		UFastGameSubsystem* Self,
		const FString& EventType,
		const FString& AdId,
		const FString& GameId,
		const FString& CampaignId,
		FLatentActionInfo LatentInfo,
		bool& bSuccess,
		int32& StatusCode,
		FString& Message);
};
