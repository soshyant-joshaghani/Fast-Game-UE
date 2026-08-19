#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "FastGameBlueprintTypes.h"
#include "FastGameListenShopProgress.generated.h"

class UFastGameSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FFastGameShopProgressOutput, bool, bOwned, const FString&, Message);

/**
 * Blueprint "Event Shop Progress": call once (BeginPlay). Exec pins fire whenever
 * shop progress happens anywhere (Unlock Sku, payment return, missing store app).
 */
UCLASS()
class FASTGAME_API UFastGameListenShopProgress : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "FastGame|Shop",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject",
			DisplayName = "Event Shop Progress"))
	static UFastGameListenShopProgress* ListenShopProgress(const UObject* WorldContextObject);

	UPROPERTY(BlueprintAssignable)
	FFastGameShopProgressOutput PurchaseSuccessful;

	UPROPERTY(BlueprintAssignable)
	FFastGameShopProgressOutput PurchasePending;

	UPROPERTY(BlueprintAssignable)
	FFastGameShopProgressOutput PurchaseFailed;

	UPROPERTY(BlueprintAssignable)
	FFastGameShopProgressOutput PurchaseCancelled;

	UPROPERTY(BlueprintAssignable)
	FFastGameShopProgressOutput StoreMissing;

	virtual void Activate() override;
	virtual void SetReadyToDestroy() override;

private:
	UFUNCTION()
	void HandleShopProgress(EFastGameShopProgress Progress, bool bOwned, const FString& Message);

	UFastGameSubsystem* ResolveSubsystem() const;

	TWeakObjectPtr<const UObject> WorldContextObjectPtr;
	TWeakObjectPtr<UFastGameSubsystem> BoundSubsystem;
};
