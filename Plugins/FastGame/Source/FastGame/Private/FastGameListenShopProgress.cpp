#include "FastGameListenShopProgress.h"
#include "FastGameSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

UFastGameListenShopProgress* UFastGameListenShopProgress::ListenShopProgress(const UObject* WorldContextObject)
{
	UFastGameListenShopProgress* Action = NewObject<UFastGameListenShopProgress>();
	Action->WorldContextObjectPtr = WorldContextObject;
	Action->RegisterWithGameInstance(WorldContextObject);
	return Action;
}

UFastGameSubsystem* UFastGameListenShopProgress::ResolveSubsystem() const
{
	const UObject* Context = WorldContextObjectPtr.Get();
	UWorld* World = nullptr;
	if (Context)
	{
		World = GEngine ? GEngine->GetWorldFromContextObject(Context, EGetWorldErrorMode::ReturnNull) : nullptr;
	}
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UFastGameSubsystem>() : nullptr;
}

void UFastGameListenShopProgress::Activate()
{
	Super::Activate();
	UFastGameSubsystem* Shop = ResolveSubsystem();
	if (!Shop)
	{
		PurchaseFailed.Broadcast(false, TEXT("FastGame: Event Shop Progress needs a world / Game Instance"));
		return;
	}
	BoundSubsystem = Shop;
	Shop->OnShopProgress.AddDynamic(this, &UFastGameListenShopProgress::HandleShopProgress);
}

void UFastGameListenShopProgress::SetReadyToDestroy()
{
	if (UFastGameSubsystem* Shop = BoundSubsystem.Get())
	{
		Shop->OnShopProgress.RemoveDynamic(this, &UFastGameListenShopProgress::HandleShopProgress);
	}
	BoundSubsystem.Reset();
	Super::SetReadyToDestroy();
}

void UFastGameListenShopProgress::HandleShopProgress(
	EFastGameShopProgress Progress, bool bOwned, const FString& Message)
{
	switch (Progress)
	{
	case EFastGameShopProgress::Success:
		PurchaseSuccessful.Broadcast(bOwned, Message);
		break;
	case EFastGameShopProgress::Pending:
		PurchasePending.Broadcast(bOwned, Message);
		break;
	case EFastGameShopProgress::Cancelled:
		PurchaseCancelled.Broadcast(bOwned, Message);
		break;
	case EFastGameShopProgress::StoreMissing:
		StoreMissing.Broadcast(bOwned, Message);
		break;
	default:
		PurchaseFailed.Broadcast(bOwned, Message);
		break;
	}
}
