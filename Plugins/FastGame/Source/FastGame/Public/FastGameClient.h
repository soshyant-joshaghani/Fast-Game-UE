#pragma once

#include "CoreMinimal.h"
#include "FastGame.h"
#include "FastGameTypes.h"
#include "FastGameHttp.h"
#include "FastGameIdentity.h"

class FASTGAME_API FFastGameAuth
{
public:
	FFastGameAuth(TSharedRef<FFastGameHttp> InHttp, FFastGameConfig InConfig)
		: Http(InHttp), Config(MoveTemp(InConfig))
	{
		LoadPersistedAccessToken();
		LoadPersistedEnteredIdentity();
	}

	bool IsLoggedIn() const { return Http->IsLoggedIn(); }
	FString GetAccessToken() const { return Http->GetAccessToken(); }
	void Logout();
	void SetAccessToken(const FString& Token);
	TFunction<void()> OnLoggedIn;
	/** Clear access token, entered identity, and pending-payment cache. Dev/login-page tool. */
	void ClearLocalCache();

	/** Persist ENTER result (normalized identity + email/phone channel). */
	void StoreEnteredIdentity(const FString& Identity, bool bIsEmail);
	/** Clear ENTER-stored identity (memory + disk). */
	void ClearEnteredIdentity();
	/** True when an ENTER identity is available (memory or disk). */
	bool HasEnteredIdentity() const;
	FString GetEnteredIdentity() const { return EnteredIdentity; }
	EFastGameIdentityChannel GetEnteredChannel() const { return EnteredChannel; }

	/**
	 * Ensure ENTER identity is loaded (memory or disk). Returns false if none.
	 * Call before Login / Signup / OTP with empty Identity.
	 */
	bool EnsureEnteredIdentityLoaded();

	/** Active catalog game for auth OTP / recovery (from Initialize Game / SetGameCode). */
	void SetGameCode(const FString& InGameCode);
	FString GetGameCode() const { return Config.GameCode; }

	/**
	 * Login with email or phone identity. Channel: Auto (detect), Email, or Phone.
	 * Empty Identity → use ENTER-stored identity (and stored channel when Channel is Auto).
	 * Hits POST /base/login/access-token (OAuth username = email or normalized phone).
	 */
	void Login(const FString& Identity, const FString& Password,
		TFunction<void(bool /*bOk*/, int32 /*StatusCode*/, FString /*AccessToken*/, FString /*Message*/)> OnDone,
		EFastGameIdentityChannel Channel = EFastGameIdentityChannel::Auto);

	/**
	 * ENTER contract: probe identity (POST /base/login/enter). No widgets — caller routes on result.
	 * On success, stores OutIdentity for Login / Signup / Recovery with empty Identity.
	 * OnDone: bOk, StatusCode, exists, password_required, channel, email, phone, message.
	 * New-user OTP: caller checks catalog auth_requirements for the active client GameCode.
	 */
	void Enter(const FString& Identity, EFastGameIdentityChannel Channel,
		TFunction<void(bool /*bOk*/, int32 /*StatusCode*/, bool /*bExists*/, bool /*bPasswordRequired*/,
			FString /*Channel*/, FString /*Email*/, FString /*Phone*/, FString /*Message*/)> OnDone);

	/**
	 * Register (email and/or phone — at least one, or empty → ENTER-stored identity).
	 * PasswordConfirm is verified locally; only Password is sent. Auto-login on success.
	 * Uses client GameCode when catalog verify is on (after signup OTP).
	 */
	void Signup(const FString& Email, const FString& Phone, const FString& Password, const FString& PasswordConfirm,
		const FString& FullName,
		TFunction<void(bool /*bOk*/, int32 /*StatusCode*/, FString /*UserId*/, FString /*Email*/, FString /*Phone*/, FString /*AccessToken*/, FString /*Message*/)> OnDone);

	/**
	 * Complete Account: set password (+ optional full name) on a passwordless existing user.
	 * Empty Email+Phone → ENTER store. Auto-login on success. No OTP.
	 */
	void CompleteAccount(const FString& Email, const FString& Phone, const FString& Password,
		const FString& PasswordConfirm, const FString& FullName,
		TFunction<void(bool /*bOk*/, int32 /*StatusCode*/, FString /*UserId*/, FString /*Email*/, FString /*Phone*/, FString /*AccessToken*/, FString /*Message*/)> OnDone);

	/** PATCH /base/login/me — display name only. Requires login. */
	void UpdateFullName(const FString& FullName,
		TFunction<void(bool /*bOk*/, int32 /*StatusCode*/, FFastGameUser /*User*/, FString /*Message*/)> OnDone);

	/** Forgot password step 1/3: send OTP. Empty Identity → ENTER-stored identity. */
	void RequestPasswordRecovery(const FString& Identity,
		TFunction<void(bool /*bOk*/, int32 /*StatusCode*/, FString /*Message*/)> OnDone);

	/** Forgot password step 2/3: verify OTP. Empty Identity → ENTER-stored identity. */
	void VerifyPasswordRecovery(const FString& Identity, const FString& Code,
		TFunction<void(bool /*bOk*/, int32 /*StatusCode*/, FString /*Message*/)> OnDone);

	/**
	 * Forgot password step 3/3: set new password after VerifyPasswordRecovery (no full name).
	 * Empty Identity → ENTER-stored identity. Pass empty Code after a successful verify.
	 * Auto-login on success (same as Register / Complete Account).
	 */
	void ConfirmPasswordRecovery(const FString& Identity, const FString& Code,
		const FString& NewPassword, const FString& NewPasswordConfirm,
		TFunction<void(bool /*bOk*/, int32 /*StatusCode*/, FString /*Message*/)> OnDone);

	/** Signup OTP step 1/2: send code for a new identity. Empty Identity → ENTER store. */
	void RequestSignupVerification(const FString& Identity,
		TFunction<void(bool /*bOk*/, int32 /*StatusCode*/, FString /*Message*/)> OnDone);

	/** Signup OTP step 2/2: verify code then show Signup. Empty Identity → ENTER store. */
	void VerifySignupVerification(const FString& Identity, const FString& Code,
		TFunction<void(bool /*bOk*/, int32 /*StatusCode*/, FString /*Message*/)> OnDone);

	/** Current user profile (no password). Requires login. */
	void GetMe(TFunction<void(bool /*bOk*/, int32 /*StatusCode*/, FFastGameUser /*User*/, FString /*Message*/)> OnDone);
	/** Bind Steam via Steamworks session ticket (uses client GameCode payment_config). */
	void LinkSteamWithTicket(const FString& Ticket, const FString& Identity,
		TFunction<void(bool /*bOk*/, bool /*bLinked*/, FString /*SteamId*/, FString /*Err*/)> OnDone);
	void GetSteamStatus(TFunction<void(bool /*bOk*/, bool /*bLinked*/, FString /*SteamId*/, FString /*Err*/)> OnDone);
	void UnlinkSteam(TFunction<void(bool, FString)> OnDone);

	/** Re-read token from disk into memory (after InitializeClient / boot). */
	void LoadPersistedAccessToken();

private:
	void PersistAccessToken(const FString& Token) const;
	void DeletePersistedAccessToken() const;
	void LoadPersistedEnteredIdentity();
	void PersistEnteredIdentity() const;
	void DeletePersistedEnteredIdentity() const;
	/** Empty Identity/Email/Phone → ENTER-stored identity. */
	bool ResolveContactFields(const FString& Identity, const FString& Email, const FString& Phone,
		FString& OutEmail, FString& OutPhone, FString& OutError);
	/** Both Email and Phone empty → fill from ENTER store (by channel). */
	bool FillEmailPhoneFromEntered(FString& InOutEmail, FString& InOutPhone, FString& OutError);
	bool RequireGameCode(FString& OutGameCode, FString& OutError) const;
	void PostContactJson(const FString& Path, const FString& Email, const FString& Phone,
		TFunction<void(TSharedPtr<FJsonObject>)> AugmentBody,
		TFunction<void(bool, int32, FString)> OnDone);
	void LoginWithEmail(const FString& Email, const FString& Password,
		TFunction<void(bool, int32, FString, FString)> OnDone);
	void LoginWithPhone(const FString& Phone, const FString& Password,
		TFunction<void(bool, int32, FString, FString)> OnDone);
	void LoginWithUsername(const FString& Username, const FString& Password,
		TFunction<void(bool, int32, FString, FString)> OnDone);

	TSharedRef<FFastGameHttp> Http;
	FFastGameConfig Config;
	FString EnteredIdentity;
	EFastGameIdentityChannel EnteredChannel = EFastGameIdentityChannel::Auto;
};

class FASTGAME_API FFastGameCatalog
{
public:
	explicit FFastGameCatalog(TSharedRef<FFastGameHttp> InHttp) : Http(InHttp) {}

	/**
	 * List games. Pass Lang (e.g. "fa") so response labels resolve for that locale.
	 * Set bExpandI18n to receive full translations maps (editor-style).
	 */
	void ListGames(bool bAvailableOnly, TFunction<void(bool, TArray<FFastGameCatalogEntry>, FString)> OnDone,
		const FString& Lang = TEXT(""), bool bExpandI18n = false);
	void GetGame(const FString& GameId, TFunction<void(bool, FFastGameCatalogDetail, FString)> OnDone,
		const FString& Lang = TEXT(""), bool bExpandI18n = false);
	/** Public auth gates for new-user OTP (no login required). */
	void GetAuthRequirements(const FString& GameId,
		TFunction<void(bool /*bOk*/, bool /*bVerifyPhone*/, bool /*bVerifyEmail*/, FString /*Err*/)> OnDone);
	/** FastAPI helper: WS URL for the sibling Colyseus SDK. */
	void GetGameServer(TFunction<void(bool, FString /*Url*/, FString)> OnDone);

private:
	TSharedRef<FFastGameHttp> Http;
};

class FASTGAME_API FFastGameContent : public TSharedFromThis<FFastGameContent>
{
public:
	FFastGameContent(TSharedRef<FFastGameHttp> InHttp, TSharedRef<FFastGameCatalog> InCatalog)
		: Http(InHttp), Catalog(InCatalog) {}

	void ListCharacters(const FString& GameId, TFunction<void(bool, TArray<FFastGameCharacter>, FString)> OnDone,
		const FString& Lang = TEXT(""), bool bExpandI18n = false);
	void ListCharacters(const FString& GameId, const FString& Role,
		TFunction<void(bool, TArray<FFastGameCharacter>, FString)> OnDone,
		const FString& Lang = TEXT(""), bool bExpandI18n = false);
	void ClaimEvent(const FString& GameId, const FString& EventId,
		TFunction<void(bool, TSharedPtr<FJsonObject>, FString)> OnDone);
	void GetMapRuntime(const FString& GameId, const FString& MapId,
		TFunction<void(bool, TSharedPtr<FJsonObject>, FString)> OnDone,
		const FString& Lang = TEXT(""), bool bExpandI18n = false);
	void ResolveSpawn(const FString& GameId, const FString& MapId, const FString& ModeId,
		TFunction<void(bool, TSharedPtr<FJsonObject>, FString)> OnDone,
		const FString& Lang = TEXT(""), bool bExpandI18n = false);
	void GetLoadout(const FString& GameId, TFunction<void(bool, FFastGameLoadout, FString)> OnDone);
	void SetLoadout(const FString& GameId, const FString& CharacterId,
		const TMap<FString, FString>& Cosmetics, const TMap<FString, FString>& ModularParts,
		TFunction<void(bool, FFastGameLoadout, FString)> OnDone);
	void ClaimPickup(const FString& GameId, const FString& MapId, const FString& PickupId, const FString& PlacementId,
		TFunction<void(bool, TSharedPtr<FJsonObject>, FString)> OnDone);
	void PrepareSession(const FString& GameId, const FString& ModeId, const FString& MapId,
		TFunction<void(bool, FFastGamePreparedSession, FString)> OnDone,
		const FString& Lang = TEXT(""), bool bExpandI18n = false);

private:
	TSharedRef<FFastGameHttp> Http;
	TSharedRef<FFastGameCatalog> Catalog;
};

class FASTGAME_API FFastGameShop
{
public:
	FFastGameShop(TSharedRef<FFastGameHttp> InHttp, FFastGameConfig InConfig)
		: Http(InHttp), Config(MoveTemp(InConfig)) {}

	bool HasPendingPayment() const;
	/** Empty GameIdFilter → Initialize Game GameCode. */
	void GetCatalog(const FString& GameIdFilter, TFunction<void(bool, TArray<FFastGameShopLine>, FString)> OnDone,
		const FString& Lang = TEXT(""), bool bExpandI18n = false);
	void SetGameCode(const FString& InGameCode) { Config.GameCode = InGameCode; }
	void SetStorePlatform(const FString& InStorePlatform)
	{
		Config.StorePlatform = FastGameNormalizeProviderId(InStorePlatform);
	}
	void SetStorePublicKey(const FString& InPublicKey) { Config.StorePublicKey = InPublicKey.TrimStartAndEnd(); }
	FString GetStorePublicKey() const { return Config.StorePublicKey; }
	FString GetGameCode() const { return Config.GameCode; }
	FString GetStorePlatform() const { return Config.StorePlatform; }

	/** Empty GameCode → Initialize Game GameCode. */
	bool ResolveGameCode(const FString& GameCode, FString& OutGameCode, FString& OutError) const;
	/** Empty Provider → Initialize Game StorePlatform. */
	bool ResolveProvider(const FString& Provider, FString& OutProvider, FString& OutError) const;
	/** Freeze Myket/Bazaar/Play wallet at login so mid-game store-account switches cannot restore. */
	void BindStoreLock();
	/**
	 * Fetch Cafe Bazaar / Myket RSA from Editor (FG1 wrap). Skips if Config.StorePublicKey is set.
	 * OnDone(true, Pem, "") even when the Editor key is missing (Pem empty). HTTP/decode errors → false.
	 */
	void EnsureStoreVerifyKey(TFunction<void(bool, FString /*Pem*/, FString /*Err*/)> OnDone);

	void GetSkuAccess(const FString& GameCode, const FString& SkuKind, const FString& SkuId,
		TFunction<void(bool, bool /*bLocked*/, bool /*bOwned*/, const TArray<FString>& /*StoreProductIds*/, FString)> OnDone);
	void ClaimFree(const FString& GameCode, const FString& SkuKind, const FString& SkuId,
		TFunction<void(bool, FString)> OnDone);
	void RedeemCode(const FString& GameCode, const FString& Code,
		TFunction<void(bool, FString)> OnDone);
	/** POST /apps/games/shop/unlock/begin. Empty GameCode → Initialize Game. Provider = StorePlatform. */
	void UnlockSku(const FString& GameCode, const FString& SkuKind, const FString& SkuId,
		const FString& CallbackUrl, const FString& DiscountCode,
		TFunction<void(bool, FFastGameShopUnlock, FString)> OnDone);
	/** POST /apps/games/shop/unlock/complete. PurchaseToken required for store IAP. */
	void CompleteUnlock(const FString& PurchaseToken, TFunction<void(bool, bool /*bOwned*/, FString)> OnDone);
	/** POST /apps/games/shop/unlock/restore — Myket/Bazaar/Play token → Fast Game owned. */
	void RestoreUnlock(const FString& GameCode, const FString& SkuKind, const FString& SkuId,
		const FString& PurchaseToken, const FString& StoreProductId,
		TFunction<void(bool, bool /*bOwned*/, FString)> OnDone);
	void Buy(const FFastGameShopLine& Line, const FString& CallbackUrl,
		TFunction<void(bool, FFastGamePaymentInitiate, FString)> OnDone);
	/** provider: zarinpal | myket | caffebazar | googleplay | steam.
	 * Store IAP (myket/caffebazar/googleplay): maps store_skus only — then native IAP + SubmitBilling.
	 * Currency pin is unused for store providers (price lives in the store console). */
	void BuyWithProvider(const FFastGameShopLine& Line, const FString& CallbackUrl,
		const FString& Provider, const FString& Currency,
		TFunction<void(bool, FFastGamePaymentInitiate, FString)> OnDone);
	void BuyWithProvider(const FFastGameShopLine& Line, const FString& CallbackUrl,
		const FString& Provider, const FString& Currency, const FString& DiscountCode,
		TFunction<void(bool, FFastGamePaymentInitiate, FString)> OnDone);
	void SubmitBilling(const FString& PurchaseToken, TFunction<void(bool, bool, FString)> OnDone);
	void FinalizeSteam(TFunction<void(bool, bool, FString)> OnDone);
	void VerifyPending(const FString& AuthorityOverride, TFunction<void(bool, bool /*bSuccess*/, FString)> OnDone);
	void ClearPendingPayment();

private:
	TSharedRef<FFastGameHttp> Http;
	FFastGameConfig Config;
};

class FASTGAME_API FFastGameAssets
{
public:
	static TArray<FFastGameAssetPack> ListPacksFromGame(const FFastGameCatalogDetail& Detail);
};

class FASTGAME_API FFastGameAds
{
public:
	explicit FFastGameAds(TSharedRef<FFastGameHttp> InHttp) : Http(InHttp) {}

	/**
	 * Request an ad. On success with no fill: bOk=true, bHasAd=false.
	 * On fill: bOk=true, bHasAd=true, Ad populated.
	 */
	void GetAdvertisement(const FFastGameAdvertisementRequest& Request,
		TFunction<void(bool /*bOk*/, bool /*bHasAd*/, FFastGameAdvertisement /*Ad*/, FString /*Err*/)> OnDone);

	void TrackEvent(const FFastGameAdvertisementEvent& Event,
		TFunction<void(bool /*bOk*/, FString /*Err*/)> OnDone);

private:
	TSharedRef<FFastGameHttp> Http;
};

/** FastAPI-named client only. Multiplayer uses sibling colyseus-unreal (or official Colyseus). */
class FASTGAME_API FFastGameClient : public TSharedFromThis<FFastGameClient>
{
public:
	explicit FFastGameClient(FFastGameConfig InConfig = FFastGameConfig());

	FFastGameConfig Config;
	TSharedRef<FFastGameHttp> Http;
	TSharedRef<FFastGameAuth> Auth;
	TSharedRef<FFastGameCatalog> Catalog;
	TSharedRef<FFastGameContent> Content;
	TSharedRef<FFastGameShop> Shop;
	TSharedRef<FFastGameAds> Ads;
};
