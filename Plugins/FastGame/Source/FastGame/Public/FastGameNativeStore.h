#pragma once

#include "CoreMinimal.h"

/**
 * Optional Android store OS (FastGameStore plugin). Fast Game does not hard-depend on FastGameStore.
 * FastGameStoreSubsystem registers itself on Initialize.
 */
class FASTGAME_API IFastGameNativeStore
{
public:
	virtual ~IFastGameNativeStore() {}

	/** Flavor + install check. PublicKey is Myket/Bazaar RSA (may be empty). */
	virtual bool EnsureSetup(const FString& ProviderId, const FString& PublicKey, FString& OutMessage) = 0;
	virtual bool IsStoreAppInstalled() const = 0;
	virtual void RequestPurchaseToken(const FString& StoreProductId, TFunction<void(FString /*Token*/, bool /*bAlreadyOwned*/)> OnDone) = 0;
	/** Inventory only (openTheStorePage=false). Empty token → not owned on store. */
	virtual void QueryStoreOwnership(const FString& StoreProductId, TFunction<void(FString /*Token*/, bool /*bAlreadyOwned*/)> OnDone) = 0;
};

class FASTGAME_API FFastGameNativeStore
{
public:
	static void Register(IFastGameNativeStore* Impl);
	static void Unregister(IFastGameNativeStore* Impl);
	static IFastGameNativeStore* Get();

	static bool IsAndroidStoreProvider(const FString& ProviderId);
};
