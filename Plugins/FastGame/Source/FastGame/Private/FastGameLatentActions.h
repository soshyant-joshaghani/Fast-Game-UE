#pragma once

#include "CoreMinimal.h"
#include "LatentActions.h"
#include "FastGameBlueprintTypes.h"

/** Shared result so HTTP callbacks stay safe if the latent action is cancelled. */
struct FFastGameRequestLatentState
{
	bool bFinished = false;
	bool bSuccess = false;
	int32 StatusCode = 0;
	FString Message;

	// Auth
	FString UserId;
	FString Email;
	FString Phone;
	FFastGameBPUser User;

	// Steam
	bool bLinked = false;
	FString SteamId;

	// Catalog
	TArray<FFastGameBPCatalogEntry> Games;
	FFastGameBPCatalogDetail Game;
	FString Url;

	// Content
	TArray<FFastGameBPCharacter> Characters;
	FFastGameBPPreparedSession Session;
	FString JsonBody;
	FFastGameBPLoadout Loadout;

	// Shop
	TArray<FFastGameBPShopLine> Lines;
	FFastGameBPPaymentInitiate Payment;
	FFastGameBPShopUnlock Unlock;
	bool bPaymentSuccess = false;
	bool bLocked = false;
	bool bOwned = false;
	bool bAuthenticated = false;
	bool bHasAd = false;
	FFastGameBPAdvertisement Ad;

	// Enter
	EFastGameEnterRoute EnterRoute = EFastGameEnterRoute::Failed;
	EFastGameShopProgress ShopProgress = EFastGameShopProgress::Failed;
	FString OutIdentity;
	bool bOutEmail = false;
	bool bOutPhone = false;
};

using FFastGameAuthLatentState = FFastGameRequestLatentState;

/** Completes a Blueprint latent pin once an HTTP call finishes. */
class FFastGameRequestLatentAction : public FPendingLatentAction
{
public:
	FName ExecutionFunction;
	int32 OutputLink;
	FWeakObjectPtr CallbackTarget;
	TSharedRef<FFastGameRequestLatentState> State;

	bool* bSuccessOut = nullptr;
	int32* StatusCodeOut = nullptr;
	FString* MessageOut = nullptr;
	FString* UserIdOut = nullptr;
	FString* EmailOut = nullptr;
	FString* PhoneOut = nullptr;
	FFastGameBPUser* UserOut = nullptr;
	bool* bLinkedOut = nullptr;
	FString* SteamIdOut = nullptr;
	TArray<FFastGameBPCatalogEntry>* GamesOut = nullptr;
	FFastGameBPCatalogDetail* GameOut = nullptr;
	FString* UrlOut = nullptr;
	TArray<FFastGameBPCharacter>* CharactersOut = nullptr;
	FFastGameBPPreparedSession* SessionOut = nullptr;
	FString* JsonBodyOut = nullptr;
	FFastGameBPLoadout* LoadoutOut = nullptr;
	TArray<FFastGameBPShopLine>* LinesOut = nullptr;
	FFastGameBPPaymentInitiate* PaymentOut = nullptr;
	FFastGameBPShopUnlock* UnlockOut = nullptr;
	bool* bPaymentSuccessOut = nullptr;
	bool* bLockedOut = nullptr;
	bool* bOwnedOut = nullptr;
	bool* bAuthenticatedOut = nullptr;
	bool* bHasAdOut = nullptr;
	FFastGameBPAdvertisement* AdOut = nullptr;
	FString* ImageUrlOut = nullptr;
	FString* VideoUrlOut = nullptr;
	FString* MediaUrlOut = nullptr;
	FString* ClickUrlOut = nullptr;
	FString* TitleOut = nullptr;
	FString* BodyOut = nullptr;
	FString* BackgroundUrlOut = nullptr;
	FString* BackgroundColorOut = nullptr;
	int32* MediaWidthOut = nullptr;
	int32* MediaHeightOut = nullptr;
	EFastGameEnterRoute* EnterRouteOut = nullptr;
	EFastGameShopProgress* ShopProgressOut = nullptr;
	FString* OutIdentityOut = nullptr;
	bool* bOutEmailOut = nullptr;
	bool* bOutPhoneOut = nullptr;
	bool bOutputsApplied = false;

	FFastGameRequestLatentAction(const FLatentActionInfo& LatentInfo, const TSharedRef<FFastGameRequestLatentState>& InState)
		: ExecutionFunction(LatentInfo.ExecutionFunction)
		, OutputLink(LatentInfo.Linkage)
		, CallbackTarget(LatentInfo.CallbackTarget)
		, State(InState)
	{
	}

	virtual void UpdateOperation(FLatentResponse& Response) override
	{
		if (State->bFinished && !bOutputsApplied)
		{
			if (bSuccessOut) *bSuccessOut = State->bSuccess;
			if (StatusCodeOut) *StatusCodeOut = State->StatusCode;
			if (MessageOut) *MessageOut = State->Message;
			if (UserIdOut) *UserIdOut = State->UserId;
			if (EmailOut) *EmailOut = State->Email;
			if (PhoneOut) *PhoneOut = State->Phone;
			if (UserOut) *UserOut = State->User;
			if (bLinkedOut) *bLinkedOut = State->bLinked;
			if (SteamIdOut) *SteamIdOut = State->SteamId;
			if (GamesOut) *GamesOut = State->Games;
			if (GameOut) *GameOut = State->Game;
			if (UrlOut) *UrlOut = State->Url;
			if (CharactersOut) *CharactersOut = State->Characters;
			if (SessionOut) *SessionOut = State->Session;
			if (JsonBodyOut) *JsonBodyOut = State->JsonBody;
			if (LoadoutOut) *LoadoutOut = State->Loadout;
			if (LinesOut) *LinesOut = State->Lines;
			if (PaymentOut) *PaymentOut = State->Payment;
			if (UnlockOut) *UnlockOut = State->Unlock;
			if (bPaymentSuccessOut) *bPaymentSuccessOut = State->bPaymentSuccess;
			if (bLockedOut) *bLockedOut = State->bLocked;
			if (bOwnedOut) *bOwnedOut = State->bOwned;
			if (bAuthenticatedOut) *bAuthenticatedOut = State->bAuthenticated;
			if (bHasAdOut) *bHasAdOut = State->bHasAd;
			if (AdOut) *AdOut = State->Ad;
			if (ImageUrlOut) *ImageUrlOut = State->Ad.MediaUrl;
			if (VideoUrlOut) *VideoUrlOut = State->Ad.MediaUrl;
			if (MediaUrlOut) *MediaUrlOut = State->Ad.MediaUrl;
			if (ClickUrlOut) *ClickUrlOut = State->Ad.ClickUrl;
			if (TitleOut) *TitleOut = State->Ad.Title;
			if (BodyOut) *BodyOut = State->Ad.Body;
			if (BackgroundUrlOut) *BackgroundUrlOut = State->Ad.BackgroundUrl;
			if (BackgroundColorOut) *BackgroundColorOut = State->Ad.BackgroundColor;
			if (MediaWidthOut) *MediaWidthOut = State->Ad.MediaWidth;
			if (MediaHeightOut) *MediaHeightOut = State->Ad.MediaHeight;
			if (EnterRouteOut) *EnterRouteOut = State->EnterRoute;
			if (ShopProgressOut) *ShopProgressOut = State->ShopProgress;
			if (OutIdentityOut) *OutIdentityOut = State->OutIdentity;
			if (bOutEmailOut) *bOutEmailOut = State->bOutEmail;
			if (bOutPhoneOut) *bOutPhoneOut = State->bOutPhone;
			bOutputsApplied = true;
		}
		Response.FinishAndTriggerIf(State->bFinished, ExecutionFunction, OutputLink, CallbackTarget);
	}

#if WITH_EDITOR
	virtual FString GetDescription() const override
	{
		return State->bFinished
			? TEXT("FastGame request finished")
			: TEXT("Waiting for FastGame request…");
	}
#endif
};

using FFastGameAuthLatentAction = FFastGameRequestLatentAction;
