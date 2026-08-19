#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FastGameNativeStore.h"
#include "FastGameStoreSubsystem.generated.h"

/** FString by value — required for AddDynamic. Internal OS callback; designers use Fast Game Unlock Sku. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnFastGameStorePurchase,
	FString, StoreProductId,
	FString, PurchaseToken,
	bool, bAlreadyOwned);

/**
 * Invisible OS machinery. Follows Fast Game Initialize Game. Designers use Fast Game only.
 */
UCLASS()
class FASTGAMESTORE_API UFastGameStoreSubsystem : public UGameInstanceSubsystem, public IFastGameNativeStore
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UPROPERTY()
	FOnFastGameStorePurchase OnStorePurchase;

	void SetStorePublicKey(const FString& PublicKey);
	FString GetStorePublicKey() const { return StorePublicKey; }
	FString GetCompiledStoreFlavor() const;
	virtual bool IsStoreAppInstalled() const override;
	bool LaunchStoreActivity(bool bOpenStorePage, const FString& StoreProductId);

	virtual bool EnsureSetup(const FString& ProviderId, const FString& PublicKey, FString& OutMessage) override;
	virtual void RequestPurchaseToken(const FString& StoreProductId, TFunction<void(FString Token, bool bAlreadyOwned)> OnDone) override;
	virtual void QueryStoreOwnership(const FString& StoreProductId, TFunction<void(FString Token, bool bAlreadyOwned)> OnDone) override;

	void BroadcastStorePurchase(FString StoreProductId, FString PurchaseToken, bool bAlreadyOwned);

private:
	void LaunchNativePurchase(const FString& StoreProductId, bool bOpenStorePage, TFunction<void(FString, bool)> OnDone);

	FString StorePublicKey;
	TFunction<void(FString /*Token*/, bool /*bAlreadyOwned*/)> PendingNative;

	/** One FastGameStoreActivity at a time — shop Access must not relaunch every tick. */
	bool bActivityOpen = false;
	FString CachedQuerySku;
	FString CachedQueryToken;
	bool CachedQueryOwned = false;
	double CachedQueryAt = 0.0;
	bool bQueuedOpenStore = false;
	FString QueuedOpenStoreSku;
	TFunction<void(FString, bool)> QueuedOpenStoreDone;
};
