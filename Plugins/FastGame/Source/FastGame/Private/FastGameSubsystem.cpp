#include "FastGameSubsystem.h"
#include "FastGameBlueprintConvert.h"
#include "FastGameClient.h"
#include "FastGameHttp.h"
#include "FastGameIdentity.h"
#include "FastGameLatentActions.h"
#include "FastGameNativeStore.h"
#include "FastGameStoreVerify.h"
#include "Async/Async.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Misc/CoreDelegates.h"
#include "Subsystems/SubsystemCollection.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace FastGameSubsystemUtil
{
	static UWorld* ResolveWorld(const UFastGameSubsystem* Subsystem)
	{
		if (!Subsystem)
		{
			return nullptr;
		}
		if (UGameInstance* GI = Subsystem->GetGameInstance())
		{
			return GI->GetWorld();
		}
		return nullptr;
	}
}

namespace FastGameSubsystemLatent
{
	struct FSetup
	{
		TSharedRef<FFastGameRequestLatentState> State;
		FFastGameRequestLatentAction* Action = nullptr;
		bool bRegistered = false;

		FSetup()
			: State(MakeShared<FFastGameRequestLatentState>())
		{
		}
	};

	static FSetup Register(
		UFastGameSubsystem* Self,
		FLatentActionInfo LatentInfo,
		int32& StatusCode,
		FString& Message,
		EFastGameRequestOutcome* OutcomeOut = nullptr)
	{
		StatusCode = 0;
		Message.Reset();
		if (OutcomeOut)
		{
			*OutcomeOut = EFastGameRequestOutcome::Failed;
		}

		FSetup Setup;

		UWorld* World = FastGameSubsystemUtil::ResolveWorld(Self);
		if (!World)
		{
			Message = TEXT("FastGame: no world for latent request");
			return Setup;
		}

		FLatentActionManager& LatentManager = World->GetLatentActionManager();
		if (LatentManager.FindExistingAction<FFastGameRequestLatentAction>(LatentInfo.CallbackTarget, LatentInfo.UUID))
		{
			return Setup;
		}

		Setup.Action = new FFastGameRequestLatentAction(LatentInfo, Setup.State);
		Setup.Action->StatusCodeOut = &StatusCode;
		Setup.Action->MessageOut = &Message;
		if (OutcomeOut)
		{
			Setup.Action->OutcomeOut = OutcomeOut;
		}
		LatentManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, Setup.Action);
		Setup.bRegistered = true;
		return Setup;
	}

	/** Catalog / content / ads latents that still expose bSuccess for Branch. */
	static FSetup RegisterWithSuccess(
		UFastGameSubsystem* Self,
		FLatentActionInfo LatentInfo,
		bool& bSuccess,
		int32& StatusCode,
		FString& Message)
	{
		bSuccess = false;
		FSetup Setup = Register(Self, LatentInfo, StatusCode, Message);
		if (Setup.bRegistered)
		{
			Setup.Action->bSuccessOut = &bSuccess;
		}
		return Setup;
	}

	static void FinishStatus(TSharedRef<FFastGameRequestLatentState> State, bool bOk, int32 Code, const FString& Msg)
	{
		State->bSuccess = bOk;
		State->StatusCode = Code;
		State->Message = Msg;
		State->Outcome = bOk ? EFastGameRequestOutcome::Success : EFastGameRequestOutcome::Failed;
		State->bFinished = true;
	}

	static EFastGameEnterPin EnterRouteToPin(EFastGameEnterRoute Route)
	{
		switch (Route)
		{
		case EFastGameEnterRoute::Login:
			return EFastGameEnterPin::EnterPassword;
		case EFastGameEnterRoute::CompleteAccount:
		case EFastGameEnterRoute::Register:
			return EFastGameEnterPin::Signup;
		case EFastGameEnterRoute::VerifyId:
			return EFastGameEnterPin::Verify;
		case EFastGameEnterRoute::Failed:
		default:
			return EFastGameEnterPin::Failed;
		}
	}

	static EFastGameShopAccessRoute ClassifyShopAccess(bool bOk, bool bOwned, bool bLocked)
	{
		if (!bOk)
		{
			return EFastGameShopAccessRoute::Failed;
		}
		if (bOwned)
		{
			return EFastGameShopAccessRoute::Owned;
		}
		if (bLocked)
		{
			return EFastGameShopAccessRoute::Locked;
		}
		return EFastGameShopAccessRoute::Available;
	}

	static void FinishErr(TSharedRef<FFastGameRequestLatentState> State, bool bOk, const FString& Err)
	{
		int32 Code = 0;
		FString Msg;
		FFastGameHttp::ParseStatusFromError(bOk, Err, Code, Msg);
		FinishStatus(State, bOk, Code, Msg);
	}

	static void SetLastRequest(UFastGameSubsystem* S, int32 Code, const FString& Msg)
	{
		if (S)
		{
			S->LastAuthStatusCode = Code;
			S->LastAuthMessage = Msg;
		}
	}
}

void UFastGameSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	TWeakObjectPtr<UFastGameSubsystem> WeakThis(this);
	auto Resume = [WeakThis]()
	{
		if (UFastGameSubsystem* S = WeakThis.Get())
		{
			S->HandleAppReactivated();
		}
	};
	AppReactivatedHandle = FCoreDelegates::ApplicationHasReactivatedDelegate.AddLambda(Resume);
	AppForegroundHandle = FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddLambda(Resume);
}

void UFastGameSubsystem::Deinitialize()
{
	if (AppReactivatedHandle.IsValid())
	{
		FCoreDelegates::ApplicationHasReactivatedDelegate.Remove(AppReactivatedHandle);
		AppReactivatedHandle.Reset();
	}
	if (AppForegroundHandle.IsValid())
	{
		FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Remove(AppForegroundHandle);
		AppForegroundHandle.Reset();
	}
	++ClientGeneration;
	Client.Reset();
	Super::Deinitialize();
}

void UFastGameSubsystem::SetShopProgress(EFastGameShopProgress Progress, bool bOwned, const FString& Message)
{
	LastShopProgress = Progress;
	if (!Message.IsEmpty())
	{
		LastAuthMessage = Message;
	}
	OnShopProgress.Broadcast(Progress, bOwned, Message);
}

bool UFastGameSubsystem::EnsureClient(FString& OutError) const
{
	if (!Client.IsValid())
	{
		OutError = TEXT("FastGame: call InitializeClient first");
		return false;
	}
	return true;
}

bool UFastGameSubsystem::EnsureStoreSetup(FString& OutMessage) const
{
	OutMessage.Empty();
	const FString Provider = GetStorePlatformId();
	IFastGameNativeStore* Native = FFastGameNativeStore::Get();
	if (Native)
	{
		return Native->EnsureSetup(Provider, StorePublicKey, OutMessage);
	}
	if (FFastGameNativeStore::IsAndroidStoreProvider(Provider))
	{
#if PLATFORM_ANDROID
		OutMessage = TEXT("FastGameStore plugin not loaded — Cafe Bazaar / Myket cannot run without it.");
		return false;
#else
		OutMessage = TEXT("FastGameStore: editor/non-Android — install check skipped");
		return true;
#endif
	}
	return true;
}

void UFastGameSubsystem::SyncNativeStorePublicKey()
{
	if (Client.IsValid())
	{
		if (StorePublicKey.IsEmpty())
		{
			if (!Client->Shop->GetStorePublicKey().IsEmpty())
			{
				StorePublicKey = Client->Shop->GetStorePublicKey();
			}
			else if (!Client->Config.StorePublicKey.IsEmpty())
			{
				StorePublicKey = Client->Config.StorePublicKey;
			}
		}
		Client->Config.StorePublicKey = StorePublicKey;
		Client->Shop->SetStorePublicKey(StorePublicKey);
	}
	FString Msg;
	EnsureStoreSetup(Msg);
}

void UFastGameSubsystem::EnsureStoreVerifyKey(TFunction<void(bool, FString)>&& OnDone)
{
	if (!StorePublicKey.IsEmpty())
	{
		SyncNativeStorePublicKey();
		OnDone(true, TEXT(""));
		return;
	}
	FString Err;
	if (!EnsureClient(Err))
	{
		OnDone(false, Err);
		return;
	}
	if (!Client->Shop->GetStorePublicKey().IsEmpty())
	{
		SetStorePublicKey(Client->Shop->GetStorePublicKey());
		SyncNativeStorePublicKey();
		OnDone(true, TEXT(""));
		return;
	}
	const int32 Gen = ClientGeneration;
	TWeakObjectPtr<UFastGameSubsystem> WeakThis(this);
	Client->Shop->EnsureStoreVerifyKey(
		[WeakThis, Gen, OnDone = MoveTemp(OnDone)](bool bOk, FString Pem, FString Error) mutable
		{
			AsyncTask(ENamedThreads::GameThread, [WeakThis, Gen, bOk, Pem, Error, OnDone = MoveTemp(OnDone)]() mutable
			{
				if (UFastGameSubsystem* S = WeakThis.Get())
				{
					if (S->ClientGeneration == Gen && bOk && !Pem.IsEmpty())
					{
						S->SetStorePublicKey(Pem);
						S->SyncNativeStorePublicKey();
					}
				}
				OnDone(bOk, Error);
			});
		});
}

void UFastGameSubsystem::ApplyPersistedGameConfig()
{
	if (!Client.IsValid())
	{
		return;
	}
	Client->Config.GameCode = PersistedGameCode;
	Client->Auth->SetGameCode(PersistedGameCode);
	Client->Shop->SetGameCode(PersistedGameCode);
	Client->Config.StorePlatform = PersistedStorePlatformId;
	Client->Shop->SetStorePlatform(PersistedStorePlatformId);
	if (!StorePublicKey.IsEmpty())
	{
		Client->Config.StorePublicKey = StorePublicKey;
		Client->Shop->SetStorePublicKey(StorePublicKey);
	}
	else if (!Client->Shop->GetStorePublicKey().IsEmpty())
	{
		StorePublicKey = Client->Shop->GetStorePublicKey();
		Client->Config.StorePublicKey = StorePublicKey;
	}
	else if (!Client->Config.StorePublicKey.IsEmpty())
	{
		StorePublicKey = Client->Config.StorePublicKey;
		Client->Shop->SetStorePublicKey(StorePublicKey);
	}
}

void UFastGameSubsystem::DispatchToGameThread(
	TWeakObjectPtr<UFastGameSubsystem> WeakThis,
	int32 Generation,
	TFunction<void(UFastGameSubsystem*)>&& Lambda)
{
	AsyncTask(ENamedThreads::GameThread, [WeakThis, Generation, Lambda = MoveTemp(Lambda)]() mutable
	{
		if (UFastGameSubsystem* Strong = WeakThis.Get())
		{
			if (Strong->ClientGeneration != Generation)
			{
				return;
			}
			Lambda(Strong);
		}
	});
}

void UFastGameSubsystem::InitializeGame(
	const FString& InGameCode,
	EFastGameStorePlatform StorePlatform,
	bool& bSuccess,
	FString& Message)
{
	bSuccess = false;
	Message.Empty();

	PersistedGameCode = InGameCode.TrimStartAndEnd();
	if (StorePlatform != EFastGameStorePlatform::Unset)
	{
		PersistedStorePlatformId = FastGameBlueprintConvert::StorePlatformToId(StorePlatform);
	}
	ApplyPersistedGameConfig();

	if (PersistedGameCode.IsEmpty())
	{
		Message = TEXT("FastGame: GameCode required — call Initialize Game");
		return;
	}

	bSuccess = EnsureStoreSetup(Message);
	if (!bSuccess)
	{
		const EFastGameShopProgress Progress = FastGameBlueprintConvert::ClassifyShopProgress(
			false, false, false, Message);
		SetShopProgress(Progress, false, Message);
		return;
	}
	if (Client.IsValid() && Client->Auth->IsLoggedIn())
	{
		Client->Shop->BindStoreLock();
	}
}

void UFastGameSubsystem::InitializeClient(
	const FString& ApiBaseUrl,
	bool& bSuccess,
	FString& Message)
{
	bSuccess = false;
	Message.Empty();

	FString Normalized = ApiBaseUrl.IsEmpty()
		? TEXT("http://api.localhost/api/v1")
		: ApiBaseUrl;
	Normalized.TrimStartAndEndInline();

	// Keep an existing client when the base URL matches — re-init was wiping the in-memory token
	// before the disk save from an in-flight Login could be reloaded.
	if (Client.IsValid()
		&& Client->Config.ApiBaseUrl.Equals(Normalized, ESearchCase::IgnoreCase))
	{
		Client->Auth->LoadPersistedAccessToken();
		ApplyPersistedGameConfig();
		bSuccess = true;
		return;
	}

	const FString CarryToken = Client.IsValid() ? Client->Auth->GetAccessToken() : FString();
	++ClientGeneration;
	NativeInventoryQueriedSkus.Empty();
	FFastGameConfig Config;
	Config.ApiBaseUrl = Normalized;
	Config.GameCode = PersistedGameCode;
	Config.StorePlatform = PersistedStorePlatformId;
	Config.StorePublicKey = StorePublicKey;
	Client = MakeShared<FFastGameClient>(Config);
	if (!CarryToken.IsEmpty())
	{
		Client->Auth->SetAccessToken(CarryToken);
	}
	bSuccess = true;
}

void UFastGameSubsystem::SetStorePublicKey(const FString& PublicKey)
{
	StorePublicKey = PublicKey.TrimStartAndEnd();
	if (Client.IsValid())
	{
		Client->Config.StorePublicKey = StorePublicKey;
		Client->Shop->SetStorePublicKey(StorePublicKey);
	}
}

FString UFastGameSubsystem::GetStorePublicKey() const
{
	return StorePublicKey;
}

void UFastGameSubsystem::SetGameCode(const FString& InGameCode)
{
	PersistedGameCode = InGameCode.TrimStartAndEnd();
	ApplyPersistedGameConfig();
}

FString UFastGameSubsystem::GetGameCode() const
{
	if (!PersistedGameCode.IsEmpty())
	{
		return PersistedGameCode;
	}
	return Client.IsValid() ? Client->Auth->GetGameCode() : FString();
}

void UFastGameSubsystem::SetStorePlatform(EFastGameStorePlatform StorePlatform)
{
	PersistedStorePlatformId = FastGameBlueprintConvert::StorePlatformToId(StorePlatform);
	ApplyPersistedGameConfig();
}

EFastGameStorePlatform UFastGameSubsystem::GetStorePlatform() const
{
	return FastGameBlueprintConvert::StorePlatformFromId(GetStorePlatformId());
}

FString UFastGameSubsystem::GetStorePlatformId() const
{
	if (!PersistedStorePlatformId.IsEmpty())
	{
		return PersistedStorePlatformId;
	}
	return Client.IsValid() ? Client->Shop->GetStorePlatform() : FString();
}

bool UFastGameSubsystem::IsInitialized() const
{
	return Client.IsValid();
}

TSharedPtr<FFastGameClient> UFastGameSubsystem::GetClient() const
{
	return Client;
}

bool UFastGameSubsystem::IsLoggedIn() const
{
	return Client.IsValid() && Client->Auth->IsLoggedIn();
}

bool UFastGameSubsystem::IsAuthenticated() const
{
	return IsLoggedIn();
}

void UFastGameSubsystem::CheckAuthentication(
	FLatentActionInfo LatentInfo,
	EFastGameAuthCheck& Check,
	int32& StatusCode,
	FString& Message)
{
	Check = EFastGameAuthCheck::Failed;

	const FastGameSubsystemLatent::FSetup Setup = FastGameSubsystemLatent::Register(this, LatentInfo, StatusCode, Message);
	if (!Setup.bRegistered)
	{
		if (!Message.IsEmpty())
		{
			FastGameSubsystemLatent::SetLastRequest(this, 0, Message);
		}
		return;
	}

	Setup.Action->AuthCheckOut = &Check;

	const TSharedRef<FFastGameRequestLatentState> State = Setup.State;

	FString Err;
	if (!EnsureClient(Err))
	{
		FastGameSubsystemLatent::SetLastRequest(this, 0, Err);
		State->AuthCheck = EFastGameAuthCheck::Failed;
		FastGameSubsystemLatent::FinishStatus(State, false, 0, Err);
		return;
	}

	if (!Client->Auth->IsLoggedIn())
	{
		State->AuthCheck = EFastGameAuthCheck::NotAuthenticated;
		FastGameSubsystemLatent::SetLastRequest(this, 401, TEXT("Not authenticated"));
		FastGameSubsystemLatent::FinishStatus(State, true, 401, TEXT("Not authenticated"));
		return;
	}

	const int32 Gen = ClientGeneration;
	TWeakObjectPtr<UFastGameSubsystem> WeakThis(this);
	Client->Auth->GetMe([WeakThis, Gen, State](bool bOk, int32 Code, FFastGameUser /*User*/, FString InMessage)
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis, Gen, bOk, Code, InMessage, State]()
		{
			if (UFastGameSubsystem* S = WeakThis.Get())
			{
				if (!bOk && S->Client.IsValid())
				{
					S->Client->Auth->Logout();
				}
				if (S->ClientGeneration == Gen)
				{
					FastGameSubsystemLatent::SetLastRequest(S, Code, InMessage);
				}
			}
			State->AuthCheck = bOk ? EFastGameAuthCheck::Authenticated : EFastGameAuthCheck::NotAuthenticated;
			FastGameSubsystemLatent::FinishStatus(State, true, Code > 0 ? Code : (bOk ? 200 : 401), InMessage);
		});
	});
}

FString UFastGameSubsystem::GetAccessToken() const
{
	return Client.IsValid() ? Client->Auth->GetAccessToken() : FString();
}

void UFastGameSubsystem::SetAccessToken(const FString& Token)
{
	if (Client.IsValid())
	{
		Client->Auth->SetAccessToken(Token);
	}
}

void UFastGameSubsystem::Logout()
{
	if (Client.IsValid())
	{
		Client->Auth->Logout();
	}
	bLastLoginSucceeded = false;
	bLastSignupSucceeded = false;
	CurrentUser = FFastGameBPUser();
	LastAuthStatusCode = 0;
	LastAuthMessage.Reset();
}

void UFastGameSubsystem::ClearLocalCache()
{
	if (Client.IsValid())
	{
		Client->Auth->ClearLocalCache();
	}
	bLastLoginSucceeded = false;
	bLastSignupSucceeded = false;
	CurrentUser = FFastGameBPUser();
	LastAuthStatusCode = 0;
	LastAuthMessage.Reset();
}

void UFastGameSubsystem::ClearEnteredIdentity()
{
	LastEnterRoute = EFastGameEnterRoute::Failed;
	if (Client.IsValid())
	{
		Client->Auth->ClearEnteredIdentity();
	}
}

FString UFastGameSubsystem::GetEnteredIdentity() const
{
	if (!Client.IsValid())
	{
		return FString();
	}
	Client->Auth->EnsureEnteredIdentityLoaded();
	return Client->Auth->GetEnteredIdentity();
}

bool UFastGameSubsystem::HasEnteredIdentity() const
{
	return Client.IsValid() && Client->Auth->HasEnteredIdentity();
}

EFastGameIdentityChannel UFastGameSubsystem::GetEnteredChannel() const
{
	return Client.IsValid() ? Client->Auth->GetEnteredChannel() : EFastGameIdentityChannel::Auto;
}

void UFastGameSubsystem::Enter(
	const FString& Identity,
	EFastGameIdentityChannel Channel,
	FLatentActionInfo LatentInfo,
	EFastGameEnterPin& Pin,
	FString& Message,
	FString& OutIdentity,
	FString& OutEmail,
	FString& OutPhone,
	bool& bOutEmail,
	bool& bOutPhone)
{
	Pin = EFastGameEnterPin::Failed;
	OutIdentity.Reset();
	OutEmail.Reset();
	OutPhone.Reset();
	bOutEmail = false;
	bOutPhone = false;

	int32 StatusCode = 0;
	const FastGameSubsystemLatent::FSetup Setup = FastGameSubsystemLatent::Register(this, LatentInfo, StatusCode, Message);
	if (!Setup.bRegistered)
	{
		if (!Message.IsEmpty())
		{
			FastGameSubsystemLatent::SetLastRequest(this, 0, Message);
		}
		return;
	}

	Setup.Action->EnterPinOut = &Pin;
	Setup.Action->OutIdentityOut = &OutIdentity;
	Setup.Action->EmailOut = &OutEmail;
	Setup.Action->PhoneOut = &OutPhone;
	Setup.Action->bOutEmailOut = &bOutEmail;
	Setup.Action->bOutPhoneOut = &bOutPhone;

	const TSharedRef<FFastGameRequestLatentState> State = Setup.State;
	TWeakObjectPtr<UFastGameSubsystem> WeakThis(this);
	auto FinishEnter = [WeakThis, State](bool bOk, int32 Code, const FString& InMessage, EFastGameEnterRoute InRoute,
		const FString& InIdentity, const FString& InEmail, const FString& InPhone, bool bInEmail, bool bInPhone)
	{
		if (UFastGameSubsystem* S = WeakThis.Get())
		{
			S->LastEnterRoute = InRoute;
		}
		State->EnterRoute = InRoute;
		State->EnterPin = FastGameSubsystemLatent::EnterRouteToPin(InRoute);
		State->OutIdentity = InIdentity;
		State->Email = InEmail;
		State->Phone = InPhone;
		State->bOutEmail = bInEmail;
		State->bOutPhone = bInPhone;
		FastGameSubsystemLatent::FinishStatus(State, bOk, Code, InMessage);
	};

	FString Err;
	if (!EnsureClient(Err))
	{
		FastGameSubsystemLatent::SetLastRequest(this, 0, Err);
		FinishEnter(false, 0, Err, EFastGameEnterRoute::Failed, TEXT(""), TEXT(""), TEXT(""), false, false);
		return;
	}

	Client->Auth->Enter(Identity, Channel,
		[this, FinishEnter](bool bOk, int32 Code, bool bExists, bool bPasswordRequired,
			FString ChannelStr, FString Email, FString Phone, FString InMessage)
		{
			if (!bOk)
			{
				AsyncTask(ENamedThreads::GameThread, [FinishEnter, Code, InMessage]()
				{
					FinishEnter(false, Code, InMessage, EFastGameEnterRoute::Failed,
						TEXT(""), TEXT(""), TEXT(""), false, false);
				});
				return;
			}

			const bool bIsEmail = ChannelStr.Equals(TEXT("email"), ESearchCase::IgnoreCase);
			const FString IdentityOut = bIsEmail ? Email : Phone;

			auto FinishRouted = [FinishEnter, Code, IdentityOut, Email, Phone, bIsEmail](EFastGameEnterRoute RouteOut)
			{
				AsyncTask(ENamedThreads::GameThread, [FinishEnter, Code, RouteOut, IdentityOut, Email, Phone, bIsEmail]()
				{
					FinishEnter(true, Code, TEXT(""), RouteOut, IdentityOut, Email, Phone, bIsEmail, !bIsEmail);
				});
			};

			if (bExists)
			{
				FinishRouted(bPasswordRequired
					? EFastGameEnterRoute::CompleteAccount
					: EFastGameEnterRoute::Login);
				return;
			}

			const FString TrimGame = Client.IsValid() ? Client->Auth->GetGameCode().TrimStartAndEnd() : FString();
			if (TrimGame.IsEmpty() || !Client.IsValid())
			{
				FinishRouted(EFastGameEnterRoute::Register);
				return;
			}

			Client->Catalog->GetAuthRequirements(TrimGame,
				[FinishRouted, bIsEmail](bool bReqOk, bool bVerifyPhone, bool bVerifyEmail, FString /*Err*/)
				{
					const bool bNeedsVerify = bReqOk && (bIsEmail ? bVerifyEmail : bVerifyPhone);
					FinishRouted(bNeedsVerify
						? EFastGameEnterRoute::VerifyId
						: EFastGameEnterRoute::Register);
				});
		});
}

void UFastGameSubsystem::Login(
	const FString& Identity,
	const FString& Password,
	EFastGameIdentityChannel Channel,
	FLatentActionInfo LatentInfo,
	EFastGameRequestOutcome& Outcome,
	int32& StatusCode,
	FString& Message)
{
	Outcome = EFastGameRequestOutcome::Failed;
	const FastGameSubsystemLatent::FSetup Setup = FastGameSubsystemLatent::Register(this, LatentInfo, StatusCode, Message, &Outcome);
	if (!Setup.bRegistered)
	{
		if (!Message.IsEmpty())
		{
			bLastLoginSucceeded = false;
			FastGameSubsystemLatent::SetLastRequest(this, 0, Message);
			OnLoginComplete.Broadcast(false, 0, Message);
		}
		return;
	}

	const TSharedRef<FFastGameRequestLatentState> State = Setup.State;
	auto Finish = [State](bool bOk, int32 Code, const FString& InMessage)
	{
		FastGameSubsystemLatent::FinishStatus(State, bOk, Code, InMessage);
	};

	FString Err;
	if (!EnsureClient(Err))
	{
		bLastLoginSucceeded = false;
		FastGameSubsystemLatent::SetLastRequest(this, 0, Err);
		OnLoginComplete.Broadcast(false, 0, Err);
		Finish(false, 0, Err);
		return;
	}

	const int32 Gen = ClientGeneration;
	TWeakObjectPtr<UFastGameSubsystem> WeakThis(this);
	Client->Auth->Login(Identity, Password, [WeakThis, Gen, Finish](bool bOk, int32 Code, FString Token, FString InMessage)
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis, Gen, bOk, Code, Token, InMessage, Finish]()
		{
			if (UFastGameSubsystem* S = WeakThis.Get())
			{
				if (bOk && !Token.IsEmpty() && S->Client.IsValid())
				{
					S->Client->Auth->SetAccessToken(Token);
				}
				if (S->ClientGeneration == Gen)
				{
					S->bLastLoginSucceeded = bOk;
					FastGameSubsystemLatent::SetLastRequest(S, Code, InMessage);
					S->OnLoginComplete.Broadcast(bOk, Code, InMessage);
				}
			}
			Finish(bOk, Code, InMessage);
		});
	}, Channel);
}

void UFastGameSubsystem::Signup(
	const FString& Email,
	const FString& Phone,
	const FString& Password,
	const FString& PasswordConfirm,
	const FString& FullName,
	FLatentActionInfo LatentInfo,
	EFastGameRequestOutcome& Outcome,
	int32& StatusCode,
	FString& UserId,
	FString& OutEmail,
	FString& OutPhone,
	FString& Message)
{
	UserId.Reset();
	OutEmail.Reset();
	OutPhone.Reset();
	Outcome = EFastGameRequestOutcome::Failed;

	const FastGameSubsystemLatent::FSetup Setup = FastGameSubsystemLatent::Register(this, LatentInfo, StatusCode, Message, &Outcome);
	if (!Setup.bRegistered)
	{
		if (!Message.IsEmpty())
		{
			bLastSignupSucceeded = false;
			bLastLoginSucceeded = false;
			FastGameSubsystemLatent::SetLastRequest(this, 0, Message);
			OnSignupComplete.Broadcast(false, 0, TEXT(""), TEXT(""), TEXT(""), Message);
		}
		return;
	}

	Setup.Action->UserIdOut = &UserId;
	Setup.Action->EmailOut = &OutEmail;
	Setup.Action->PhoneOut = &OutPhone;

	const TSharedRef<FFastGameRequestLatentState> State = Setup.State;
	auto Finish = [State](bool bOk, int32 Code, const FString& InUserId, const FString& InEmail, const FString& InPhone, const FString& InMessage)
	{
		State->UserId = InUserId;
		State->Email = InEmail;
		State->Phone = InPhone;
		FastGameSubsystemLatent::FinishStatus(State, bOk, Code, InMessage);
	};

	FString Err;
	if (!EnsureClient(Err))
	{
		bLastSignupSucceeded = false;
		bLastLoginSucceeded = false;
		FastGameSubsystemLatent::SetLastRequest(this, 0, Err);
		OnSignupComplete.Broadcast(false, 0, TEXT(""), TEXT(""), TEXT(""), Err);
		Finish(false, 0, TEXT(""), TEXT(""), TEXT(""), Err);
		return;
	}

	const int32 Gen = ClientGeneration;
	TWeakObjectPtr<UFastGameSubsystem> WeakThis(this);
	auto OnDone = [WeakThis, Gen, Finish](bool bOk, int32 Code, FString InUserId, FString InEmail, FString InPhone, FString Token, FString InMessage)
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis, Gen, bOk, Code, InUserId, InEmail, InPhone, Token, InMessage, Finish]()
		{
			if (UFastGameSubsystem* S = WeakThis.Get())
			{
				if (bOk && !Token.IsEmpty() && S->Client.IsValid())
				{
					S->Client->Auth->SetAccessToken(Token);
				}
				if (S->ClientGeneration == Gen)
				{
					S->bLastSignupSucceeded = bOk;
					S->bLastLoginSucceeded = bOk;
					FastGameSubsystemLatent::SetLastRequest(S, Code, InMessage);
					S->OnSignupComplete.Broadcast(bOk, Code, InUserId, InEmail, InPhone, InMessage);
					if (bOk)
					{
						S->OnLoginComplete.Broadcast(true, Code, TEXT(""));
					}
				}
			}
			Finish(bOk, Code, InUserId, InEmail, InPhone, InMessage);
		});
	};

	// Seeded password_required: same credentials UI, /complete under the hood.
	if (LastEnterRoute == EFastGameEnterRoute::CompleteAccount)
	{
		Client->Auth->CompleteAccount(Email, Phone, Password, PasswordConfirm, FullName, OnDone);
	}
	else
	{
		Client->Auth->Signup(Email, Phone, Password, PasswordConfirm, FullName, OnDone);
	}
}

void UFastGameSubsystem::ConfirmPasswordRecovery(
	const FString& Identity,
	const FString& NewPassword,
	const FString& NewPasswordConfirm,
	FLatentActionInfo LatentInfo,
	EFastGameRequestOutcome& Outcome,
	int32& StatusCode,
	FString& Message)
{
	Outcome = EFastGameRequestOutcome::Failed;
	const FastGameSubsystemLatent::FSetup Setup = FastGameSubsystemLatent::Register(this, LatentInfo, StatusCode, Message, &Outcome);
	if (!Setup.bRegistered)
	{
		return;
	}

	const TSharedRef<FFastGameRequestLatentState> State = Setup.State;
	auto Finish = [State](bool bOk, int32 Status, const FString& InMessage)
	{
		FastGameSubsystemLatent::FinishStatus(State, bOk, Status, InMessage);
	};

	FString Err;
	if (!EnsureClient(Err))
	{
		FastGameSubsystemLatent::SetLastRequest(this, 0, Err);
		Finish(false, 0, Err);
		return;
	}

	// Step 3 after Verify — empty Code; backend uses the already-verified challenge.
	Client->Auth->ConfirmPasswordRecovery(Identity, TEXT(""), NewPassword, NewPasswordConfirm,
		[Finish](bool bOk, int32 Status, FString InMessage)
		{
			AsyncTask(ENamedThreads::GameThread, [Finish, bOk, Status, InMessage]()
			{
				Finish(bOk, Status, InMessage);
			});
		});
}

void UFastGameSubsystem::SendAuthCode(
	const FString& Identity,
	FLatentActionInfo LatentInfo,
	EFastGameRequestOutcome& Outcome,
	int32& StatusCode,
	FString& Message)
{
	Outcome = EFastGameRequestOutcome::Failed;
	const FastGameSubsystemLatent::FSetup Setup = FastGameSubsystemLatent::Register(this, LatentInfo, StatusCode, Message, &Outcome);
	if (!Setup.bRegistered)
	{
		return;
	}

	const TSharedRef<FFastGameRequestLatentState> State = Setup.State;
	auto Finish = [State](bool bOk, int32 Code, const FString& InMessage)
	{
		FastGameSubsystemLatent::FinishStatus(State, bOk, Code, InMessage);
	};

	FString Err;
	if (!EnsureClient(Err))
	{
		FastGameSubsystemLatent::SetLastRequest(this, 0, Err);
		Finish(false, 0, Err);
		return;
	}

	if (LastEnterRoute == EFastGameEnterRoute::VerifyId)
	{
		Client->Auth->RequestSignupVerification(Identity,
			[Finish](bool bOk, int32 Code, FString InMessage)
			{
				AsyncTask(ENamedThreads::GameThread, [Finish, bOk, Code, InMessage]()
				{
					Finish(bOk, Code, InMessage);
				});
			});
		return;
	}
	// Enter Password screen → forgot-password recovery OTP (no Begin Forgot).
	if (LastEnterRoute == EFastGameEnterRoute::Login)
	{
		Client->Auth->RequestPasswordRecovery(Identity,
			[Finish](bool bOk, int32 Code, FString InMessage)
			{
				AsyncTask(ENamedThreads::GameThread, [Finish, bOk, Code, InMessage]()
				{
					Finish(bOk, Code, InMessage);
				});
			});
		return;
	}
	const FString Hint = TEXT("Send Auth Code: use after Enter → Verify, or from Enter Password (forgot)");
	FastGameSubsystemLatent::SetLastRequest(this, 0, Hint);
	Finish(false, 0, Hint);
}

void UFastGameSubsystem::VerifyAuthCode(
	const FString& Identity,
	const FString& Code,
	FLatentActionInfo LatentInfo,
	EFastGameVerifyAuthPin& Pin,
	int32& StatusCode,
	FString& Message)
{
	Pin = EFastGameVerifyAuthPin::Failed;
	const FastGameSubsystemLatent::FSetup Setup = FastGameSubsystemLatent::Register(this, LatentInfo, StatusCode, Message);
	if (!Setup.bRegistered)
	{
		return;
	}

	Setup.Action->VerifyAuthPinOut = &Pin;

	const TSharedRef<FFastGameRequestLatentState> State = Setup.State;
	TWeakObjectPtr<UFastGameSubsystem> WeakThis(this);
	auto Finish = [WeakThis, State](bool bOk, int32 Status, const FString& InMessage)
	{
		EFastGameVerifyAuthPin OutPin = EFastGameVerifyAuthPin::Failed;
		if (bOk)
		{
			if (UFastGameSubsystem* S = WeakThis.Get())
			{
				if (S->LastEnterRoute == EFastGameEnterRoute::VerifyId)
				{
					OutPin = EFastGameVerifyAuthPin::Signup;
				}
				else if (S->LastEnterRoute == EFastGameEnterRoute::Login)
				{
					OutPin = EFastGameVerifyAuthPin::AssignNewPassword;
				}
			}
		}
		State->VerifyAuthPin = OutPin;
		FastGameSubsystemLatent::FinishStatus(State, bOk && OutPin != EFastGameVerifyAuthPin::Failed, Status, InMessage);
	};

	FString Err;
	if (!EnsureClient(Err))
	{
		FastGameSubsystemLatent::SetLastRequest(this, 0, Err);
		Finish(false, 0, Err);
		return;
	}

	if (LastEnterRoute == EFastGameEnterRoute::VerifyId)
	{
		Client->Auth->VerifySignupVerification(Identity, Code,
			[Finish](bool bOk, int32 Status, FString InMessage)
			{
				AsyncTask(ENamedThreads::GameThread, [Finish, bOk, Status, InMessage]()
				{
					Finish(bOk, Status, InMessage);
				});
			});
		return;
	}
	if (LastEnterRoute == EFastGameEnterRoute::Login)
	{
		Client->Auth->VerifyPasswordRecovery(Identity, Code,
			[Finish](bool bOk, int32 Status, FString InMessage)
			{
				AsyncTask(ENamedThreads::GameThread, [Finish, bOk, Status, InMessage]()
				{
					Finish(bOk, Status, InMessage);
				});
			});
		return;
	}
	const FString Hint = TEXT("Verify Auth Code: use after Enter → Verify, or from Enter Password (forgot)");
	FastGameSubsystemLatent::SetLastRequest(this, 0, Hint);
	Finish(false, 0, Hint);
}

bool UFastGameSubsystem::IsEmailIdentity(const FString& Identity)
{
	return FastGameIdentity::LooksLikeEmail(Identity);
}

bool UFastGameSubsystem::IsPhoneIdentity(const FString& Identity)
{
	return FastGameIdentity::LooksLikePhone(Identity);
}

void UFastGameSubsystem::GetMe(
	FLatentActionInfo LatentInfo,
	EFastGameRequestOutcome& Outcome,
	int32& StatusCode,
	FFastGameBPUser& User,
	FString& Message)
{
	User = FFastGameBPUser();
	Outcome = EFastGameRequestOutcome::Failed;

	const FastGameSubsystemLatent::FSetup Setup = FastGameSubsystemLatent::Register(this, LatentInfo, StatusCode, Message, &Outcome);
	if (!Setup.bRegistered)
	{
		if (!Message.IsEmpty())
		{
			FastGameSubsystemLatent::SetLastRequest(this, 0, Message);
			OnGetMeComplete.Broadcast(false, 0, User, Message);
		}
		return;
	}

	Setup.Action->UserOut = &User;

	const TSharedRef<FFastGameRequestLatentState> State = Setup.State;
	auto Finish = [State](bool bOk, int32 Code, const FFastGameBPUser& InUser, const FString& InMessage)
	{
		State->User = InUser;
		FastGameSubsystemLatent::FinishStatus(State, bOk, Code, InMessage);
	};

	FString Err;
	if (!EnsureClient(Err))
	{
		FastGameSubsystemLatent::SetLastRequest(this, 0, Err);
		OnGetMeComplete.Broadcast(false, 0, FFastGameBPUser(), Err);
		Finish(false, 0, FFastGameBPUser(), Err);
		return;
	}
	if (!Client->Auth->IsLoggedIn())
	{
		Err = TEXT("Not logged in");
		FastGameSubsystemLatent::SetLastRequest(this, 401, Err);
		OnGetMeComplete.Broadcast(false, 401, FFastGameBPUser(), Err);
		Finish(false, 401, FFastGameBPUser(), Err);
		return;
	}

	const int32 Gen = ClientGeneration;
	TWeakObjectPtr<UFastGameSubsystem> WeakThis(this);
	Client->Auth->GetMe([WeakThis, Gen, Finish](bool bOk, int32 Code, FFastGameUser NativeUser, FString InMessage)
	{
		FFastGameBPUser Bp = FastGameBlueprintConvert::ToBP(NativeUser);
		AsyncTask(ENamedThreads::GameThread, [WeakThis, Gen, bOk, Code, Bp = MoveTemp(Bp), InMessage, Finish]() mutable
		{
			if (UFastGameSubsystem* S = WeakThis.Get())
			{
				if (S->ClientGeneration == Gen)
				{
					if (bOk)
					{
						S->CurrentUser = Bp;
					}
					FastGameSubsystemLatent::SetLastRequest(S, Code, InMessage);
					S->OnGetMeComplete.Broadcast(bOk, Code, Bp, InMessage);
				}
			}
			Finish(bOk, Code, Bp, InMessage);
		});
	});
}

void UFastGameSubsystem::UpdateFullName(
	const FString& FullName,
	FLatentActionInfo LatentInfo,
	EFastGameRequestOutcome& Outcome,
	int32& StatusCode,
	FFastGameBPUser& User,
	FString& Message)
{
	User = FFastGameBPUser();
	Outcome = EFastGameRequestOutcome::Failed;

	const FastGameSubsystemLatent::FSetup Setup = FastGameSubsystemLatent::Register(this, LatentInfo, StatusCode, Message, &Outcome);
	if (!Setup.bRegistered)
	{
		if (!Message.IsEmpty())
		{
			FastGameSubsystemLatent::SetLastRequest(this, 0, Message);
			OnGetMeComplete.Broadcast(false, 0, User, Message);
		}
		return;
	}

	Setup.Action->UserOut = &User;

	const TSharedRef<FFastGameRequestLatentState> State = Setup.State;
	auto Finish = [State](bool bOk, int32 Code, const FFastGameBPUser& InUser, const FString& InMessage)
	{
		State->User = InUser;
		FastGameSubsystemLatent::FinishStatus(State, bOk, Code, InMessage);
	};

	FString Err;
	if (!EnsureClient(Err))
	{
		FastGameSubsystemLatent::SetLastRequest(this, 0, Err);
		OnGetMeComplete.Broadcast(false, 0, FFastGameBPUser(), Err);
		Finish(false, 0, FFastGameBPUser(), Err);
		return;
	}
	if (!Client->Auth->IsLoggedIn())
	{
		Err = TEXT("Not logged in");
		FastGameSubsystemLatent::SetLastRequest(this, 401, Err);
		OnGetMeComplete.Broadcast(false, 401, FFastGameBPUser(), Err);
		Finish(false, 401, FFastGameBPUser(), Err);
		return;
	}

	const int32 Gen = ClientGeneration;
	TWeakObjectPtr<UFastGameSubsystem> WeakThis(this);
	Client->Auth->UpdateFullName(FullName, [WeakThis, Gen, Finish](bool bOk, int32 Code, FFastGameUser NativeUser, FString InMessage)
	{
		FFastGameBPUser Bp = FastGameBlueprintConvert::ToBP(NativeUser);
		AsyncTask(ENamedThreads::GameThread, [WeakThis, Gen, bOk, Code, Bp = MoveTemp(Bp), InMessage, Finish]() mutable
		{
			if (UFastGameSubsystem* S = WeakThis.Get())
			{
				if (S->ClientGeneration == Gen)
				{
					if (bOk)
					{
						S->CurrentUser = Bp;
					}
					FastGameSubsystemLatent::SetLastRequest(S, Code, InMessage);
					S->OnGetMeComplete.Broadcast(bOk, Code, Bp, InMessage);
				}
			}
			Finish(bOk, Code, Bp, InMessage);
		});
	});
}

void UFastGameSubsystem::LinkSteamWithTicket(
	const FString& Ticket,
	const FString& Identity,
	FLatentActionInfo LatentInfo,
	EFastGameRequestOutcome& Outcome,
	int32& StatusCode,
	FString& Message,
	bool& bLinked,
	FString& SteamId)
{
	bLinked = false;
	SteamId.Reset();
	Outcome = EFastGameRequestOutcome::Failed;

	const FastGameSubsystemLatent::FSetup Setup = FastGameSubsystemLatent::Register(this, LatentInfo, StatusCode, Message, &Outcome);
	if (!Setup.bRegistered)
	{
		if (!Message.IsEmpty())
		{
			FastGameSubsystemLatent::SetLastRequest(this, 0, Message);
			OnSteamLinkComplete.Broadcast(false, false, TEXT(""), Message);
		}
		return;
	}

	Setup.Action->bLinkedOut = &bLinked;
	Setup.Action->SteamIdOut = &SteamId;

	const TSharedRef<FFastGameRequestLatentState> State = Setup.State;
	FString Err;
	if (!EnsureClient(Err))
	{
		FastGameSubsystemLatent::SetLastRequest(this, 0, Err);
		OnSteamLinkComplete.Broadcast(false, false, TEXT(""), Err);
		FastGameSubsystemLatent::FinishErr(State, false, Err);
		return;
	}

	const int32 Gen = ClientGeneration;
	TWeakObjectPtr<UFastGameSubsystem> WeakThis(this);
	Client->Auth->LinkSteamWithTicket(Ticket, Identity,
		[WeakThis, Gen, State](bool bOk, bool bInLinked, FString InSteamId, FString Error)
		{
			int32 Code = 0;
			FString Msg;
			FFastGameHttp::ParseStatusFromError(bOk, Error, Code, Msg);
			AsyncTask(ENamedThreads::GameThread, [WeakThis, Gen, bOk, bInLinked, InSteamId, Error, Code, Msg, State]()
			{
				if (UFastGameSubsystem* S = WeakThis.Get())
				{
					if (S->ClientGeneration == Gen)
					{
						FastGameSubsystemLatent::SetLastRequest(S, Code, Msg);
						S->OnSteamLinkComplete.Broadcast(bOk, bInLinked, InSteamId, Error);
					}
				}
				State->bLinked = bInLinked;
				State->SteamId = InSteamId;
				FastGameSubsystemLatent::FinishStatus(State, bOk, Code, Msg);
			});
		});
}

void UFastGameSubsystem::GetSteamStatus(
	FLatentActionInfo LatentInfo,
	EFastGameRequestOutcome& Outcome,
	int32& StatusCode,
	FString& Message,
	bool& bLinked,
	FString& SteamId)
{
	bLinked = false;
	SteamId.Reset();
	Outcome = EFastGameRequestOutcome::Failed;

	const FastGameSubsystemLatent::FSetup Setup = FastGameSubsystemLatent::Register(this, LatentInfo, StatusCode, Message, &Outcome);
	if (!Setup.bRegistered)
	{
		if (!Message.IsEmpty())
		{
			FastGameSubsystemLatent::SetLastRequest(this, 0, Message);
			OnSteamStatus.Broadcast(false, false, TEXT(""), Message);
		}
		return;
	}

	Setup.Action->bLinkedOut = &bLinked;
	Setup.Action->SteamIdOut = &SteamId;

	const TSharedRef<FFastGameRequestLatentState> State = Setup.State;
	FString Err;
	if (!EnsureClient(Err))
	{
		FastGameSubsystemLatent::SetLastRequest(this, 0, Err);
		OnSteamStatus.Broadcast(false, false, TEXT(""), Err);
		FastGameSubsystemLatent::FinishErr(State, false, Err);
		return;
	}

	const int32 Gen = ClientGeneration;
	TWeakObjectPtr<UFastGameSubsystem> WeakThis(this);
	Client->Auth->GetSteamStatus([WeakThis, Gen, State](bool bOk, bool bInLinked, FString InSteamId, FString Error)
	{
		int32 Code = 0;
		FString Msg;
		FFastGameHttp::ParseStatusFromError(bOk, Error, Code, Msg);
		AsyncTask(ENamedThreads::GameThread, [WeakThis, Gen, bOk, bInLinked, InSteamId, Error, Code, Msg, State]()
		{
			if (UFastGameSubsystem* S = WeakThis.Get())
			{
				if (S->ClientGeneration == Gen)
				{
					FastGameSubsystemLatent::SetLastRequest(S, Code, Msg);
					S->OnSteamStatus.Broadcast(bOk, bInLinked, InSteamId, Error);
				}
			}
			State->bLinked = bInLinked;
			State->SteamId = InSteamId;
			FastGameSubsystemLatent::FinishStatus(State, bOk, Code, Msg);
		});
	});
}

void UFastGameSubsystem::UnlinkSteam(
	FLatentActionInfo LatentInfo,
	EFastGameRequestOutcome& Outcome,
	int32& StatusCode,
	FString& Message)
{
	Outcome = EFastGameRequestOutcome::Failed;
	const FastGameSubsystemLatent::FSetup Setup = FastGameSubsystemLatent::Register(this, LatentInfo, StatusCode, Message, &Outcome);
	if (!Setup.bRegistered)
	{
		if (!Message.IsEmpty())
		{
			FastGameSubsystemLatent::SetLastRequest(this, 0, Message);
			OnUnlinkSteamComplete.Broadcast(false, Message);
		}
		return;
	}

	const TSharedRef<FFastGameRequestLatentState> State = Setup.State;
	FString Err;
	if (!EnsureClient(Err))
	{
		FastGameSubsystemLatent::SetLastRequest(this, 0, Err);
		OnUnlinkSteamComplete.Broadcast(false, Err);
		FastGameSubsystemLatent::FinishErr(State, false, Err);
		return;
	}

	const int32 Gen = ClientGeneration;
	TWeakObjectPtr<UFastGameSubsystem> WeakThis(this);
	Client->Auth->UnlinkSteam([WeakThis, Gen, State](bool bOk, FString Error)
	{
		int32 Code = 0;
		FString Msg;
		FFastGameHttp::ParseStatusFromError(bOk, Error, Code, Msg);
		AsyncTask(ENamedThreads::GameThread, [WeakThis, Gen, bOk, Error, Code, Msg, State]()
		{
			if (UFastGameSubsystem* S = WeakThis.Get())
			{
				if (S->ClientGeneration == Gen)
				{
					FastGameSubsystemLatent::SetLastRequest(S, Code, Msg);
					S->OnUnlinkSteamComplete.Broadcast(bOk, Error);
				}
			}
			FastGameSubsystemLatent::FinishStatus(State, bOk, Code, Msg);
		});
	});
}

void UFastGameSubsystem::ListGames(
	bool bAvailableOnly,
	const FString& Lang,
	bool bExpandI18n,
	FLatentActionInfo LatentInfo,
	bool& bSuccess,
	int32& StatusCode,
	FString& Message,
	TArray<FFastGameBPCatalogEntry>& Games)
{
	Games.Reset();

	const FastGameSubsystemLatent::FSetup Setup = FastGameSubsystemLatent::RegisterWithSuccess(this, LatentInfo, bSuccess, StatusCode, Message);
	if (!Setup.bRegistered)
	{
		if (!Message.IsEmpty())
		{
			FastGameSubsystemLatent::SetLastRequest(this, 0, Message);
			OnListGamesComplete.Broadcast(false, {}, Message);
		}
		return;
	}

	Setup.Action->GamesOut = &Games;

	const TSharedRef<FFastGameRequestLatentState> State = Setup.State;
	FString Err;
	if (!EnsureClient(Err))
	{
		FastGameSubsystemLatent::SetLastRequest(this, 0, Err);
		OnListGamesComplete.Broadcast(false, {}, Err);
		FastGameSubsystemLatent::FinishErr(State, false, Err);
		return;
	}

	const int32 Gen = ClientGeneration;
	TWeakObjectPtr<UFastGameSubsystem> WeakThis(this);
	Client->Catalog->ListGames(bAvailableOnly,
		[WeakThis, Gen, State](bool bOk, TArray<FFastGameCatalogEntry> InGames, FString Error)
		{
			TArray<FFastGameBPCatalogEntry> Bp = FastGameBlueprintConvert::ToBPArray(InGames);
			int32 Code = 0;
			FString Msg;
			FFastGameHttp::ParseStatusFromError(bOk, Error, Code, Msg);
			AsyncTask(ENamedThreads::GameThread, [WeakThis, Gen, bOk, Bp = MoveTemp(Bp), Error, Code, Msg, State]() mutable
			{
				if (UFastGameSubsystem* S = WeakThis.Get())
				{
					if (S->ClientGeneration == Gen)
					{
						FastGameSubsystemLatent::SetLastRequest(S, Code, Msg);
						S->OnListGamesComplete.Broadcast(bOk, Bp, Error);
					}
				}
				State->Games = Bp;
				FastGameSubsystemLatent::FinishStatus(State, bOk, Code, Msg);
			});
		}, Lang, bExpandI18n);
}

void UFastGameSubsystem::GetGame(
	const FString& GameId,
	const FString& Lang,
	bool bExpandI18n,
	FLatentActionInfo LatentInfo,
	bool& bSuccess,
	int32& StatusCode,
	FString& Message,
	FFastGameBPCatalogDetail& Game)
{
	Game = FFastGameBPCatalogDetail();

	const FastGameSubsystemLatent::FSetup Setup = FastGameSubsystemLatent::RegisterWithSuccess(this, LatentInfo, bSuccess, StatusCode, Message);
	if (!Setup.bRegistered)
	{
		if (!Message.IsEmpty())
		{
			FastGameSubsystemLatent::SetLastRequest(this, 0, Message);
			OnGetGameComplete.Broadcast(false, Game, Message);
		}
		return;
	}

	Setup.Action->GameOut = &Game;

	const TSharedRef<FFastGameRequestLatentState> State = Setup.State;
	FString Err;
	if (!EnsureClient(Err))
	{
		FastGameSubsystemLatent::SetLastRequest(this, 0, Err);
		OnGetGameComplete.Broadcast(false, FFastGameBPCatalogDetail(), Err);
		FastGameSubsystemLatent::FinishErr(State, false, Err);
		return;
	}

	const int32 Gen = ClientGeneration;
	TWeakObjectPtr<UFastGameSubsystem> WeakThis(this);
	Client->Catalog->GetGame(GameId,
		[WeakThis, Gen, State](bool bOk, FFastGameCatalogDetail InGame, FString Error)
		{
			FFastGameBPCatalogDetail Bp = FastGameBlueprintConvert::ToBP(InGame);
			int32 Code = 0;
			FString Msg;
			FFastGameHttp::ParseStatusFromError(bOk, Error, Code, Msg);
			AsyncTask(ENamedThreads::GameThread, [WeakThis, Gen, bOk, Bp = MoveTemp(Bp), Error, Code, Msg, State]() mutable
			{
				if (UFastGameSubsystem* S = WeakThis.Get())
				{
					if (S->ClientGeneration == Gen)
					{
						FastGameSubsystemLatent::SetLastRequest(S, Code, Msg);
						S->OnGetGameComplete.Broadcast(bOk, Bp, Error);
					}
				}
				State->Game = Bp;
				FastGameSubsystemLatent::FinishStatus(State, bOk, Code, Msg);
			});
		}, Lang, bExpandI18n);
}

void UFastGameSubsystem::GetGameServer(
	FLatentActionInfo LatentInfo,
	bool& bSuccess,
	int32& StatusCode,
	FString& Message,
	FString& Url)
{
	Url.Reset();

	const FastGameSubsystemLatent::FSetup Setup = FastGameSubsystemLatent::RegisterWithSuccess(this, LatentInfo, bSuccess, StatusCode, Message);
	if (!Setup.bRegistered)
	{
		if (!Message.IsEmpty())
		{
			FastGameSubsystemLatent::SetLastRequest(this, 0, Message);
			OnGetGameServerComplete.Broadcast(false, TEXT(""), Message);
		}
		return;
	}

	Setup.Action->UrlOut = &Url;

	const TSharedRef<FFastGameRequestLatentState> State = Setup.State;
	FString Err;
	if (!EnsureClient(Err))
	{
		FastGameSubsystemLatent::SetLastRequest(this, 0, Err);
		OnGetGameServerComplete.Broadcast(false, TEXT(""), Err);
		FastGameSubsystemLatent::FinishErr(State, false, Err);
		return;
	}

	const int32 Gen = ClientGeneration;
	TWeakObjectPtr<UFastGameSubsystem> WeakThis(this);
	Client->Catalog->GetGameServer([WeakThis, Gen, State](bool bOk, FString InUrl, FString Error)
	{
		int32 Code = 0;
		FString Msg;
		FFastGameHttp::ParseStatusFromError(bOk, Error, Code, Msg);
		AsyncTask(ENamedThreads::GameThread, [WeakThis, Gen, bOk, InUrl, Error, Code, Msg, State]()
		{
			if (UFastGameSubsystem* S = WeakThis.Get())
			{
				if (S->ClientGeneration == Gen)
				{
					FastGameSubsystemLatent::SetLastRequest(S, Code, Msg);
					S->OnGetGameServerComplete.Broadcast(bOk, InUrl, Error);
				}
			}
			State->Url = InUrl;
			FastGameSubsystemLatent::FinishStatus(State, bOk, Code, Msg);
		});
	});
}

void UFastGameSubsystem::GetBootstrap(
	const FString& GameCode,
	FLatentActionInfo LatentInfo,
	bool& bSuccess,
	int32& StatusCode,
	FString& Message,
	FString& JsonBody)
{
	JsonBody.Reset();

	const FastGameSubsystemLatent::FSetup Setup = FastGameSubsystemLatent::RegisterWithSuccess(this, LatentInfo, bSuccess, StatusCode, Message);
	if (!Setup.bRegistered)
	{
		if (!Message.IsEmpty())
		{
			FastGameSubsystemLatent::SetLastRequest(this, 0, Message);
			OnGetBootstrapComplete.Broadcast(false, TEXT(""), Message);
		}
		return;
	}

	Setup.Action->JsonBodyOut = &JsonBody;

	const TSharedRef<FFastGameRequestLatentState> State = Setup.State;
	FString Err;
	if (!EnsureClient(Err))
	{
		FastGameSubsystemLatent::SetLastRequest(this, 0, Err);
		OnGetBootstrapComplete.Broadcast(false, TEXT(""), Err);
		FastGameSubsystemLatent::FinishErr(State, false, Err);
		return;
	}

	const int32 Gen = ClientGeneration;
	TWeakObjectPtr<UFastGameSubsystem> WeakThis(this);
	Client->Content->GetBootstrap(GameCode,
		[WeakThis, Gen, State](bool bOk, TSharedPtr<FJsonObject> Json, FString Error)
		{
			FString Body = FastGameBlueprintConvert::JsonObjectToString(Json);
			int32 Code = 0;
			FString Msg;
			FFastGameHttp::ParseStatusFromError(bOk, Error, Code, Msg);
			AsyncTask(ENamedThreads::GameThread, [WeakThis, Gen, bOk, Body, Error, Code, Msg, State]()
			{
				if (UFastGameSubsystem* S = WeakThis.Get())
				{
					if (S->ClientGeneration == Gen)
					{
						FastGameSubsystemLatent::SetLastRequest(S, Code, Msg);
						S->OnGetBootstrapComplete.Broadcast(bOk, Body, Error);
					}
				}
				State->JsonBody = Body;
				FastGameSubsystemLatent::FinishStatus(State, bOk, Code, Msg);
			});
		});
}

void UFastGameSubsystem::GetGameConfig(
	const FString& GameCode,
	FLatentActionInfo LatentInfo,
	bool& bSuccess,
	int32& StatusCode,
	FString& Message,
	FString& JsonBody)
{
	JsonBody.Reset();

	const FastGameSubsystemLatent::FSetup Setup = FastGameSubsystemLatent::RegisterWithSuccess(this, LatentInfo, bSuccess, StatusCode, Message);
	if (!Setup.bRegistered)
	{
		if (!Message.IsEmpty())
		{
			FastGameSubsystemLatent::SetLastRequest(this, 0, Message);
			OnGetGameConfigComplete.Broadcast(false, TEXT(""), Message);
		}
		return;
	}

	Setup.Action->JsonBodyOut = &JsonBody;

	const TSharedRef<FFastGameRequestLatentState> State = Setup.State;
	FString Err;
	if (!EnsureClient(Err))
	{
		FastGameSubsystemLatent::SetLastRequest(this, 0, Err);
		OnGetGameConfigComplete.Broadcast(false, TEXT(""), Err);
		FastGameSubsystemLatent::FinishErr(State, false, Err);
		return;
	}

	const int32 Gen = ClientGeneration;
	TWeakObjectPtr<UFastGameSubsystem> WeakThis(this);
	Client->Content->GetGameConfig(GameCode,
		[WeakThis, Gen, State](bool bOk, TSharedPtr<FJsonObject> Json, FString Error)
		{
			FString Body = FastGameBlueprintConvert::JsonObjectToString(Json);
			int32 Code = 0;
			FString Msg;
			FFastGameHttp::ParseStatusFromError(bOk, Error, Code, Msg);
			AsyncTask(ENamedThreads::GameThread, [WeakThis, Gen, bOk, Body, Error, Code, Msg, State]()
			{
				if (UFastGameSubsystem* S = WeakThis.Get())
				{
					if (S->ClientGeneration == Gen)
					{
						FastGameSubsystemLatent::SetLastRequest(S, Code, Msg);
						S->OnGetGameConfigComplete.Broadcast(bOk, Body, Error);
					}
				}
				State->JsonBody = Body;
				FastGameSubsystemLatent::FinishStatus(State, bOk, Code, Msg);
			});
		});
}

void UFastGameSubsystem::GetMapConfig(
	const FString& GameCode,
	const FString& MapId,
	FLatentActionInfo LatentInfo,
	bool& bSuccess,
	int32& StatusCode,
	FString& Message,
	FString& JsonBody)
{
	JsonBody.Reset();

	const FastGameSubsystemLatent::FSetup Setup = FastGameSubsystemLatent::RegisterWithSuccess(this, LatentInfo, bSuccess, StatusCode, Message);
	if (!Setup.bRegistered)
	{
		if (!Message.IsEmpty())
		{
			FastGameSubsystemLatent::SetLastRequest(this, 0, Message);
			OnGetMapConfigComplete.Broadcast(false, TEXT(""), Message);
		}
		return;
	}

	Setup.Action->JsonBodyOut = &JsonBody;

	const TSharedRef<FFastGameRequestLatentState> State = Setup.State;
	FString Err;
	if (!EnsureClient(Err))
	{
		FastGameSubsystemLatent::SetLastRequest(this, 0, Err);
		OnGetMapConfigComplete.Broadcast(false, TEXT(""), Err);
		FastGameSubsystemLatent::FinishErr(State, false, Err);
		return;
	}

	const int32 Gen = ClientGeneration;
	TWeakObjectPtr<UFastGameSubsystem> WeakThis(this);
	Client->Content->GetMapConfig(GameCode, MapId,
		[WeakThis, Gen, State](bool bOk, TSharedPtr<FJsonObject> Json, FString Error)
		{
			FString Body = FastGameBlueprintConvert::JsonObjectToString(Json);
			int32 Code = 0;
			FString Msg;
			FFastGameHttp::ParseStatusFromError(bOk, Error, Code, Msg);
			AsyncTask(ENamedThreads::GameThread, [WeakThis, Gen, bOk, Body, Error, Code, Msg, State]()
			{
				if (UFastGameSubsystem* S = WeakThis.Get())
				{
					if (S->ClientGeneration == Gen)
					{
						FastGameSubsystemLatent::SetLastRequest(S, Code, Msg);
						S->OnGetMapConfigComplete.Broadcast(bOk, Body, Error);
					}
				}
				State->JsonBody = Body;
				FastGameSubsystemLatent::FinishStatus(State, bOk, Code, Msg);
			});
		});
}

void UFastGameSubsystem::ListCharacters(
	const FString& GameId,
	const FString& Role,
	const FString& Lang,
	bool bExpandI18n,
	FLatentActionInfo LatentInfo,
	bool& bSuccess,
	int32& StatusCode,
	FString& Message,
	TArray<FFastGameBPCharacter>& Characters)
{
	Characters.Reset();

	const FastGameSubsystemLatent::FSetup Setup = FastGameSubsystemLatent::RegisterWithSuccess(this, LatentInfo, bSuccess, StatusCode, Message);
	if (!Setup.bRegistered)
	{
		if (!Message.IsEmpty())
		{
			FastGameSubsystemLatent::SetLastRequest(this, 0, Message);
			OnListCharactersComplete.Broadcast(false, {}, Message);
		}
		return;
	}

	Setup.Action->CharactersOut = &Characters;

	const TSharedRef<FFastGameRequestLatentState> State = Setup.State;
	FString Err;
	if (!EnsureClient(Err))
	{
		FastGameSubsystemLatent::SetLastRequest(this, 0, Err);
		OnListCharactersComplete.Broadcast(false, {}, Err);
		FastGameSubsystemLatent::FinishErr(State, false, Err);
		return;
	}

	const int32 Gen = ClientGeneration;
	TWeakObjectPtr<UFastGameSubsystem> WeakThis(this);
	auto Handler = [WeakThis, Gen, State](bool bOk, TArray<FFastGameCharacter> Chars, FString Error)
	{
		TArray<FFastGameBPCharacter> Bp = FastGameBlueprintConvert::ToBPArray(Chars);
		int32 Code = 0;
		FString Msg;
		FFastGameHttp::ParseStatusFromError(bOk, Error, Code, Msg);
		AsyncTask(ENamedThreads::GameThread, [WeakThis, Gen, bOk, Bp = MoveTemp(Bp), Error, Code, Msg, State]() mutable
		{
			if (UFastGameSubsystem* S = WeakThis.Get())
			{
				if (S->ClientGeneration == Gen)
				{
					FastGameSubsystemLatent::SetLastRequest(S, Code, Msg);
					S->OnListCharactersComplete.Broadcast(bOk, Bp, Error);
				}
			}
			State->Characters = Bp;
			FastGameSubsystemLatent::FinishStatus(State, bOk, Code, Msg);
		});
	};
	if (Role.IsEmpty())
	{
		Client->Content->ListCharacters(GameId, Handler, Lang, bExpandI18n);
	}
	else
	{
		Client->Content->ListCharacters(GameId, Role, Handler, Lang, bExpandI18n);
	}
}

void UFastGameSubsystem::PrepareSession(
	const FString& GameId,
	const FString& ModeId,
	const FString& MapId,
	const FString& Lang,
	bool bExpandI18n,
	FLatentActionInfo LatentInfo,
	bool& bSuccess,
	int32& StatusCode,
	FString& Message,
	FFastGameBPPreparedSession& Session)
{
	Session = FFastGameBPPreparedSession();

	const FastGameSubsystemLatent::FSetup Setup = FastGameSubsystemLatent::RegisterWithSuccess(this, LatentInfo, bSuccess, StatusCode, Message);
	if (!Setup.bRegistered)
	{
		if (!Message.IsEmpty())
		{
			FastGameSubsystemLatent::SetLastRequest(this, 0, Message);
			OnPrepareSessionComplete.Broadcast(false, Session, Message);
		}
		return;
	}

	Setup.Action->SessionOut = &Session;

	const TSharedRef<FFastGameRequestLatentState> State = Setup.State;
	FString Err;
	if (!EnsureClient(Err))
	{
		FastGameSubsystemLatent::SetLastRequest(this, 0, Err);
		OnPrepareSessionComplete.Broadcast(false, FFastGameBPPreparedSession(), Err);
		FastGameSubsystemLatent::FinishErr(State, false, Err);
		return;
	}

	const int32 Gen = ClientGeneration;
	TWeakObjectPtr<UFastGameSubsystem> WeakThis(this);
	Client->Content->PrepareSession(GameId, ModeId, MapId,
		[WeakThis, Gen, State](bool bOk, FFastGamePreparedSession InSession, FString Error)
		{
			FFastGameBPPreparedSession Bp = FastGameBlueprintConvert::ToBP(InSession);
			int32 Code = 0;
			FString Msg;
			FFastGameHttp::ParseStatusFromError(bOk, Error, Code, Msg);
			AsyncTask(ENamedThreads::GameThread, [WeakThis, Gen, bOk, Bp = MoveTemp(Bp), Error, Code, Msg, State]() mutable
			{
				if (UFastGameSubsystem* S = WeakThis.Get())
				{
					if (S->ClientGeneration == Gen)
					{
						FastGameSubsystemLatent::SetLastRequest(S, Code, Msg);
						S->OnPrepareSessionComplete.Broadcast(bOk, Bp, Error);
					}
				}
				State->Session = Bp;
				FastGameSubsystemLatent::FinishStatus(State, bOk, Code, Msg);
			});
		}, Lang, bExpandI18n);
}

void UFastGameSubsystem::GetMapRuntime(
	const FString& GameId,
	const FString& MapId,
	const FString& Lang,
	bool bExpandI18n,
	FLatentActionInfo LatentInfo,
	bool& bSuccess,
	int32& StatusCode,
	FString& Message,
	FString& JsonBody)
{
	JsonBody.Reset();

	const FastGameSubsystemLatent::FSetup Setup = FastGameSubsystemLatent::RegisterWithSuccess(this, LatentInfo, bSuccess, StatusCode, Message);
	if (!Setup.bRegistered)
	{
		if (!Message.IsEmpty())
		{
			FastGameSubsystemLatent::SetLastRequest(this, 0, Message);
			OnMapRuntimeComplete.Broadcast(false, TEXT(""), Message);
		}
		return;
	}

	Setup.Action->JsonBodyOut = &JsonBody;

	const TSharedRef<FFastGameRequestLatentState> State = Setup.State;
	FString Err;
	if (!EnsureClient(Err))
	{
		FastGameSubsystemLatent::SetLastRequest(this, 0, Err);
		OnMapRuntimeComplete.Broadcast(false, TEXT(""), Err);
		FastGameSubsystemLatent::FinishErr(State, false, Err);
		return;
	}

	const int32 Gen = ClientGeneration;
	TWeakObjectPtr<UFastGameSubsystem> WeakThis(this);
	Client->Content->GetMapRuntime(GameId, MapId,
		[WeakThis, Gen, State](bool bOk, TSharedPtr<FJsonObject> Json, FString Error)
		{
			FString Body = FastGameBlueprintConvert::JsonObjectToString(Json);
			int32 Code = 0;
			FString Msg;
			FFastGameHttp::ParseStatusFromError(bOk, Error, Code, Msg);
			AsyncTask(ENamedThreads::GameThread, [WeakThis, Gen, bOk, Body, Error, Code, Msg, State]()
			{
				if (UFastGameSubsystem* S = WeakThis.Get())
				{
					if (S->ClientGeneration == Gen)
					{
						FastGameSubsystemLatent::SetLastRequest(S, Code, Msg);
						S->OnMapRuntimeComplete.Broadcast(bOk, Body, Error);
					}
				}
				State->JsonBody = Body;
				FastGameSubsystemLatent::FinishStatus(State, bOk, Code, Msg);
			});
		}, Lang, bExpandI18n);
}

void UFastGameSubsystem::ResolveSpawn(
	const FString& GameId,
	const FString& MapId,
	const FString& ModeId,
	const FString& Lang,
	bool bExpandI18n,
	FLatentActionInfo LatentInfo,
	bool& bSuccess,
	int32& StatusCode,
	FString& Message,
	FString& JsonBody)
{
	JsonBody.Reset();

	const FastGameSubsystemLatent::FSetup Setup = FastGameSubsystemLatent::RegisterWithSuccess(this, LatentInfo, bSuccess, StatusCode, Message);
	if (!Setup.bRegistered)
	{
		if (!Message.IsEmpty())
		{
			FastGameSubsystemLatent::SetLastRequest(this, 0, Message);
			OnResolveSpawnComplete.Broadcast(false, TEXT(""), Message);
		}
		return;
	}

	Setup.Action->JsonBodyOut = &JsonBody;

	const TSharedRef<FFastGameRequestLatentState> State = Setup.State;
	FString Err;
	if (!EnsureClient(Err))
	{
		FastGameSubsystemLatent::SetLastRequest(this, 0, Err);
		OnResolveSpawnComplete.Broadcast(false, TEXT(""), Err);
		FastGameSubsystemLatent::FinishErr(State, false, Err);
		return;
	}

	const int32 Gen = ClientGeneration;
	TWeakObjectPtr<UFastGameSubsystem> WeakThis(this);
	Client->Content->ResolveSpawn(GameId, MapId, ModeId,
		[WeakThis, Gen, State](bool bOk, TSharedPtr<FJsonObject> Json, FString Error)
		{
			FString Body = FastGameBlueprintConvert::JsonObjectToString(Json);
			int32 Code = 0;
			FString Msg;
			FFastGameHttp::ParseStatusFromError(bOk, Error, Code, Msg);
			AsyncTask(ENamedThreads::GameThread, [WeakThis, Gen, bOk, Body, Error, Code, Msg, State]()
			{
				if (UFastGameSubsystem* S = WeakThis.Get())
				{
					if (S->ClientGeneration == Gen)
					{
						FastGameSubsystemLatent::SetLastRequest(S, Code, Msg);
						S->OnResolveSpawnComplete.Broadcast(bOk, Body, Error);
					}
				}
				State->JsonBody = Body;
				FastGameSubsystemLatent::FinishStatus(State, bOk, Code, Msg);
			});
		}, Lang, bExpandI18n);
}

void UFastGameSubsystem::GetLoadout(
	const FString& GameId,
	FLatentActionInfo LatentInfo,
	bool& bSuccess,
	int32& StatusCode,
	FString& Message,
	FFastGameBPLoadout& Loadout)
{
	Loadout = FFastGameBPLoadout();

	const FastGameSubsystemLatent::FSetup Setup = FastGameSubsystemLatent::RegisterWithSuccess(this, LatentInfo, bSuccess, StatusCode, Message);
	if (!Setup.bRegistered)
	{
		if (!Message.IsEmpty())
		{
			FastGameSubsystemLatent::SetLastRequest(this, 0, Message);
			OnGetLoadoutComplete.Broadcast(false, Loadout, Message);
		}
		return;
	}

	Setup.Action->LoadoutOut = &Loadout;

	const TSharedRef<FFastGameRequestLatentState> State = Setup.State;
	FString Err;
	if (!EnsureClient(Err))
	{
		FastGameSubsystemLatent::SetLastRequest(this, 0, Err);
		OnGetLoadoutComplete.Broadcast(false, FFastGameBPLoadout(), Err);
		FastGameSubsystemLatent::FinishErr(State, false, Err);
		return;
	}

	const int32 Gen = ClientGeneration;
	TWeakObjectPtr<UFastGameSubsystem> WeakThis(this);
	Client->Content->GetLoadout(GameId,
		[WeakThis, Gen, State](bool bOk, FFastGameLoadout InLoadout, FString Error)
		{
			FFastGameBPLoadout Bp = FastGameBlueprintConvert::ToBP(InLoadout);
			int32 Code = 0;
			FString Msg;
			FFastGameHttp::ParseStatusFromError(bOk, Error, Code, Msg);
			AsyncTask(ENamedThreads::GameThread, [WeakThis, Gen, bOk, Bp = MoveTemp(Bp), Error, Code, Msg, State]() mutable
			{
				if (UFastGameSubsystem* S = WeakThis.Get())
				{
					if (S->ClientGeneration == Gen)
					{
						FastGameSubsystemLatent::SetLastRequest(S, Code, Msg);
						S->OnGetLoadoutComplete.Broadcast(bOk, Bp, Error);
					}
				}
				State->Loadout = Bp;
				FastGameSubsystemLatent::FinishStatus(State, bOk, Code, Msg);
			});
		});
}

void UFastGameSubsystem::SetLoadout(
	const FString& GameId,
	const FString& CharacterId,
	const TMap<FString, FString>& Cosmetics,
	const TMap<FString, FString>& ModularParts,
	FLatentActionInfo LatentInfo,
	bool& bSuccess,
	int32& StatusCode,
	FString& Message,
	FFastGameBPLoadout& Loadout)
{
	Loadout = FFastGameBPLoadout();

	const FastGameSubsystemLatent::FSetup Setup = FastGameSubsystemLatent::RegisterWithSuccess(this, LatentInfo, bSuccess, StatusCode, Message);
	if (!Setup.bRegistered)
	{
		if (!Message.IsEmpty())
		{
			FastGameSubsystemLatent::SetLastRequest(this, 0, Message);
			OnSetLoadoutComplete.Broadcast(false, Loadout, Message);
		}
		return;
	}

	Setup.Action->LoadoutOut = &Loadout;

	const TSharedRef<FFastGameRequestLatentState> State = Setup.State;
	FString Err;
	if (!EnsureClient(Err))
	{
		FastGameSubsystemLatent::SetLastRequest(this, 0, Err);
		OnSetLoadoutComplete.Broadcast(false, FFastGameBPLoadout(), Err);
		FastGameSubsystemLatent::FinishErr(State, false, Err);
		return;
	}

	const int32 Gen = ClientGeneration;
	TWeakObjectPtr<UFastGameSubsystem> WeakThis(this);
	Client->Content->SetLoadout(GameId, CharacterId, Cosmetics, ModularParts,
		[WeakThis, Gen, State](bool bOk, FFastGameLoadout InLoadout, FString Error)
		{
			FFastGameBPLoadout Bp = FastGameBlueprintConvert::ToBP(InLoadout);
			int32 Code = 0;
			FString Msg;
			FFastGameHttp::ParseStatusFromError(bOk, Error, Code, Msg);
			AsyncTask(ENamedThreads::GameThread, [WeakThis, Gen, bOk, Bp = MoveTemp(Bp), Error, Code, Msg, State]() mutable
			{
				if (UFastGameSubsystem* S = WeakThis.Get())
				{
					if (S->ClientGeneration == Gen)
					{
						FastGameSubsystemLatent::SetLastRequest(S, Code, Msg);
						S->OnSetLoadoutComplete.Broadcast(bOk, Bp, Error);
					}
				}
				State->Loadout = Bp;
				FastGameSubsystemLatent::FinishStatus(State, bOk, Code, Msg);
			});
		});
}

void UFastGameSubsystem::ClaimPickup(
	const FString& GameId,
	const FString& MapId,
	const FString& PickupId,
	const FString& PlacementId,
	FLatentActionInfo LatentInfo,
	bool& bSuccess,
	int32& StatusCode,
	FString& Message,
	FString& JsonBody)
{
	JsonBody.Reset();

	const FastGameSubsystemLatent::FSetup Setup = FastGameSubsystemLatent::RegisterWithSuccess(this, LatentInfo, bSuccess, StatusCode, Message);
	if (!Setup.bRegistered)
	{
		if (!Message.IsEmpty())
		{
			FastGameSubsystemLatent::SetLastRequest(this, 0, Message);
			OnClaimPickupComplete.Broadcast(false, TEXT(""), Message);
		}
		return;
	}

	Setup.Action->JsonBodyOut = &JsonBody;

	const TSharedRef<FFastGameRequestLatentState> State = Setup.State;
	FString Err;
	if (!EnsureClient(Err))
	{
		FastGameSubsystemLatent::SetLastRequest(this, 0, Err);
		OnClaimPickupComplete.Broadcast(false, TEXT(""), Err);
		FastGameSubsystemLatent::FinishErr(State, false, Err);
		return;
	}

	const int32 Gen = ClientGeneration;
	TWeakObjectPtr<UFastGameSubsystem> WeakThis(this);
	Client->Content->ClaimPickup(GameId, MapId, PickupId, PlacementId,
		[WeakThis, Gen, State](bool bOk, TSharedPtr<FJsonObject> Json, FString Error)
		{
			FString Body = FastGameBlueprintConvert::JsonObjectToString(Json);
			int32 Code = 0;
			FString Msg;
			FFastGameHttp::ParseStatusFromError(bOk, Error, Code, Msg);
			AsyncTask(ENamedThreads::GameThread, [WeakThis, Gen, bOk, Body, Error, Code, Msg, State]()
			{
				if (UFastGameSubsystem* S = WeakThis.Get())
				{
					if (S->ClientGeneration == Gen)
					{
						FastGameSubsystemLatent::SetLastRequest(S, Code, Msg);
						S->OnClaimPickupComplete.Broadcast(bOk, Body, Error);
					}
				}
				State->JsonBody = Body;
				FastGameSubsystemLatent::FinishStatus(State, bOk, Code, Msg);
			});
		});
}

void UFastGameSubsystem::ClaimEvent(
	const FString& GameId,
	const FString& EventId,
	FLatentActionInfo LatentInfo,
	bool& bSuccess,
	int32& StatusCode,
	FString& Message,
	FString& JsonBody)
{
	JsonBody.Reset();

	const FastGameSubsystemLatent::FSetup Setup = FastGameSubsystemLatent::RegisterWithSuccess(this, LatentInfo, bSuccess, StatusCode, Message);
	if (!Setup.bRegistered)
	{
		if (!Message.IsEmpty())
		{
			FastGameSubsystemLatent::SetLastRequest(this, 0, Message);
			OnClaimEventComplete.Broadcast(false, TEXT(""), Message);
		}
		return;
	}

	Setup.Action->JsonBodyOut = &JsonBody;

	const TSharedRef<FFastGameRequestLatentState> State = Setup.State;
	FString Err;
	if (!EnsureClient(Err))
	{
		FastGameSubsystemLatent::SetLastRequest(this, 0, Err);
		OnClaimEventComplete.Broadcast(false, TEXT(""), Err);
		FastGameSubsystemLatent::FinishErr(State, false, Err);
		return;
	}

	const int32 Gen = ClientGeneration;
	TWeakObjectPtr<UFastGameSubsystem> WeakThis(this);
	Client->Content->ClaimEvent(GameId, EventId,
		[WeakThis, Gen, State](bool bOk, TSharedPtr<FJsonObject> Json, FString Error)
		{
			FString Body = FastGameBlueprintConvert::JsonObjectToString(Json);
			int32 Code = 0;
			FString Msg;
			FFastGameHttp::ParseStatusFromError(bOk, Error, Code, Msg);
			AsyncTask(ENamedThreads::GameThread, [WeakThis, Gen, bOk, Body, Error, Code, Msg, State]()
			{
				if (UFastGameSubsystem* S = WeakThis.Get())
				{
					if (S->ClientGeneration == Gen)
					{
						FastGameSubsystemLatent::SetLastRequest(S, Code, Msg);
						S->OnClaimEventComplete.Broadcast(bOk, Body, Error);
					}
				}
				State->JsonBody = Body;
				FastGameSubsystemLatent::FinishStatus(State, bOk, Code, Msg);
			});
		});
}

bool UFastGameSubsystem::HasPendingPayment() const
{
	return Client.IsValid() && Client->Shop->HasPendingPayment();
}

void UFastGameSubsystem::GetShopCatalog(
	const FString& GameIdFilter,
	const FString& Lang,
	bool bExpandI18n,
	FLatentActionInfo LatentInfo,
	EFastGameRequestOutcome& Outcome,
	int32& StatusCode,
	FString& Message,
	TArray<FFastGameBPShopLine>& Lines)
{
	Lines.Reset();
	Outcome = EFastGameRequestOutcome::Failed;

	const FastGameSubsystemLatent::FSetup Setup = FastGameSubsystemLatent::Register(this, LatentInfo, StatusCode, Message, &Outcome);
	if (!Setup.bRegistered)
	{
		if (!Message.IsEmpty())
		{
			FastGameSubsystemLatent::SetLastRequest(this, 0, Message);
			OnShopCatalogComplete.Broadcast(false, {}, Message);
		}
		return;
	}

	Setup.Action->LinesOut = &Lines;

	const TSharedRef<FFastGameRequestLatentState> State = Setup.State;
	FString Err;
	if (!EnsureClient(Err))
	{
		FastGameSubsystemLatent::SetLastRequest(this, 0, Err);
		OnShopCatalogComplete.Broadcast(false, {}, Err);
		FastGameSubsystemLatent::FinishErr(State, false, Err);
		return;
	}

	const int32 Gen = ClientGeneration;
	TWeakObjectPtr<UFastGameSubsystem> WeakThis(this);
	Client->Shop->GetCatalog(GameIdFilter,
		[WeakThis, Gen, State](bool bOk, TArray<FFastGameShopLine> InLines, FString Error)
		{
			TArray<FFastGameBPShopLine> Bp = FastGameBlueprintConvert::ToBPArray(InLines);
			int32 Code = 0;
			FString Msg;
			FFastGameHttp::ParseStatusFromError(bOk, Error, Code, Msg);
			AsyncTask(ENamedThreads::GameThread, [WeakThis, Gen, bOk, Bp = MoveTemp(Bp), Error, Code, Msg, State]() mutable
			{
				if (UFastGameSubsystem* S = WeakThis.Get())
				{
					if (S->ClientGeneration == Gen)
					{
						FastGameSubsystemLatent::SetLastRequest(S, Code, Msg);
						S->OnShopCatalogComplete.Broadcast(bOk, Bp, Error);
					}
				}
				State->Lines = Bp;
				FastGameSubsystemLatent::FinishStatus(State, bOk, Code, Msg);
			});
		}, Lang, bExpandI18n);
}

bool UFastGameSubsystem::IsShopLineLocked(const FFastGameBPShopLine& Line) const
{
	if (Line.MetaJson.IsEmpty())
	{
		return false;
	}
	TSharedPtr<FJsonObject> Obj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Line.MetaJson);
	if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid())
	{
		return false;
	}
	bool bLocked = false;
	Obj->TryGetBoolField(TEXT("locked"), bLocked);
	return bLocked;
}

bool UFastGameSubsystem::IsShopLineOwned(const FFastGameBPShopLine& Line) const
{
	return Line.bOwned;
}

void UFastGameSubsystem::GetShopSkuAccess(
	const FString& GameCode,
	const FString& SkuKind,
	const FString& SkuId,
	FLatentActionInfo LatentInfo,
	EFastGameShopAccessRoute& Access,
	int32& StatusCode,
	FString& Message)
{
	Access = EFastGameShopAccessRoute::Failed;

	const FastGameSubsystemLatent::FSetup Setup = FastGameSubsystemLatent::Register(this, LatentInfo, StatusCode, Message);
	if (!Setup.bRegistered)
	{
		if (!Message.IsEmpty())
		{
			FastGameSubsystemLatent::SetLastRequest(this, 0, Message);
			OnShopSkuAccessComplete.Broadcast(false, false, false, Message);
		}
		return;
	}

	Setup.Action->ShopAccessRouteOut = &Access;

	const TSharedRef<FFastGameRequestLatentState> State = Setup.State;
	FString Err;
	if (!EnsureClient(Err))
	{
		FastGameSubsystemLatent::SetLastRequest(this, 0, Err);
		OnShopSkuAccessComplete.Broadcast(false, false, false, Err);
		State->ShopAccessRoute = EFastGameShopAccessRoute::Failed;
		FastGameSubsystemLatent::FinishErr(State, false, Err);
		return;
	}

	const FString AccessGame = GameCode;
	const FString AccessKind = SkuKind;
	const FString AccessId = SkuId;
	const int32 Gen = ClientGeneration;
	TWeakObjectPtr<UFastGameSubsystem> WeakThis(this);
	EnsureStoreVerifyKey([WeakThis, Gen, State, AccessGame, AccessKind, AccessId](bool /*bKeyOk*/, FString /*KeyErr*/)
	{
		UFastGameSubsystem* Host = WeakThis.Get();
		if (!Host || Host->ClientGeneration != Gen || !Host->Client.IsValid())
		{
			FastGameSubsystemLatent::FinishErr(State, false, TEXT("FastGame destroyed"));
			return;
		}
		Host->Client->Shop->GetSkuAccess(AccessGame, AccessKind, AccessId,
		[WeakThis, Gen, State, AccessGame, AccessKind, AccessId](
			bool bOk, bool Locked, bool Owned, const TArray<FString>& StoreProductIds, FString Error)
		{
			auto FinishAccess = [WeakThis, Gen, State](
				bool bOkInner, bool LockedInner, bool OwnedInner, const FString& ErrorInner)
			{
				int32 Code = 0;
				FString Msg = ErrorInner;
				FFastGameHttp::ParseStatusFromError(bOkInner, ErrorInner, Code, Msg);
				if (bOkInner && ErrorInner.IsEmpty())
				{
					Msg.Empty();
					Code = 200;
				}
				else if (!ErrorInner.IsEmpty())
				{
					Msg = ErrorInner;
					if (Code == 0)
					{
						Code = 502;
					}
				}
				AsyncTask(ENamedThreads::GameThread, [WeakThis, Gen, bOkInner, LockedInner, OwnedInner, Msg, Code, State]()
				{
					if (UFastGameSubsystem* S = WeakThis.Get())
					{
						if (S->ClientGeneration == Gen)
						{
							FastGameSubsystemLatent::SetLastRequest(S, Code, Msg);
							if (!Msg.IsEmpty())
							{
								UE_LOG(LogTemp, Warning, TEXT("FastGame shop access: %s"), *Msg);
							}
							S->OnShopSkuAccessComplete.Broadcast(bOkInner, LockedInner, OwnedInner, Msg);
						}
					}
					State->bLocked = LockedInner;
					State->bOwned = OwnedInner;
					const bool bAccessOk = bOkInner && Msg.IsEmpty();
					State->ShopAccessRoute = FastGameSubsystemLatent::ClassifyShopAccess(
						bAccessOk, OwnedInner, LockedInner);
					FastGameSubsystemLatent::FinishStatus(State, bAccessOk, Code, Msg);
				});
			};

			if (!bOk || Owned || StoreProductIds.Num() == 0)
			{
				FinishAccess(bOk, Locked, Owned, Error);
				return;
			}

			AsyncTask(ENamedThreads::GameThread, [WeakThis, Gen, Locked, StoreProductIds, AccessGame, AccessKind, AccessId, FinishAccess]()
			{
				UFastGameSubsystem* S = WeakThis.Get();
				if (!S || S->ClientGeneration != Gen || !S->Client.IsValid())
				{
					FinishAccess(true, Locked, false, TEXT(""));
					return;
				}
				if (!FFastGameNativeStore::IsAndroidStoreProvider(S->Client->Shop->GetStorePlatform()))
				{
					FinishAccess(true, Locked, false, TEXT(""));
					return;
				}
				IFastGameNativeStore* Native = FFastGameNativeStore::Get();
				if (!Native || !Native->IsStoreAppInstalled())
				{
					FinishAccess(true, Locked, false, TEXT(""));
					return;
				}

				TSharedRef<TArray<FString>> PendingIds = MakeShared<TArray<FString>>();
				for (const FString& Id : StoreProductIds)
				{
					const FString Trimmed = Id.TrimStartAndEnd();
					if (Trimmed.IsEmpty())
					{
						continue;
					}
					const FString QueryKey = Trimmed.ToLower();
					if (S->NativeInventoryQueriedSkus.Contains(QueryKey))
					{
						continue;
					}
					S->NativeInventoryQueriedSkus.Add(QueryKey);
					PendingIds->Add(Trimmed);
				}
				if (PendingIds->Num() == 0)
				{
					FinishAccess(true, Locked, false, TEXT(""));
					return;
				}

				TSharedRef<int32> NextIndex = MakeShared<int32>(0);
				TSharedRef<FString> LastRestoreError = MakeShared<FString>();
				TSharedPtr<TFunction<void()>> TryNext = MakeShared<TFunction<void()>>();
				*TryNext = [WeakThis, Gen, Locked, AccessGame, AccessKind, AccessId, FinishAccess, PendingIds, NextIndex, LastRestoreError, TryNext, Native]()
				{
					if (*NextIndex >= PendingIds->Num())
					{
						FinishAccess(true, Locked, false, *LastRestoreError);
						return;
					}
					const FString ProductId = (*PendingIds)[(*NextIndex)++];
					Native->QueryStoreOwnership(ProductId,
						[WeakThis, Gen, Locked, AccessGame, AccessKind, AccessId, ProductId, FinishAccess, LastRestoreError, TryNext](
							FString Token, bool /*bAlready*/)
						{
							if (Token.TrimStartAndEnd().IsEmpty())
							{
								(*TryNext)();
								return;
							}
							UFastGameSubsystem* S2 = WeakThis.Get();
							if (!S2 || S2->ClientGeneration != Gen || !S2->Client.IsValid())
							{
								FinishAccess(true, Locked, false, *LastRestoreError);
								return;
							}
							S2->Client->Shop->RestoreUnlock(
								AccessGame, AccessKind, AccessId, Token, ProductId,
								[Locked, FinishAccess, LastRestoreError, TryNext](
									bool /*bHttpOk*/, bool bNowOwned, FString DoneErr)
								{
									if (bNowOwned)
									{
										FinishAccess(true, Locked, true, TEXT(""));
										return;
									}
									if (!DoneErr.IsEmpty())
									{
										*LastRestoreError = DoneErr;
									}
									(*TryNext)();
								});
						});
				};
				(*TryNext)();
			});
		});
	});
}

void UFastGameSubsystem::ClaimFree(
	const FString& GameCode,
	const FString& SkuKind,
	const FString& SkuId,
	FLatentActionInfo LatentInfo,
	EFastGameRequestOutcome& Outcome,
	int32& StatusCode,
	FString& Message)
{
	Outcome = EFastGameRequestOutcome::Failed;
	const FastGameSubsystemLatent::FSetup Setup = FastGameSubsystemLatent::Register(this, LatentInfo, StatusCode, Message, &Outcome);
	if (!Setup.bRegistered)
	{
		if (!Message.IsEmpty())
		{
			FastGameSubsystemLatent::SetLastRequest(this, 0, Message);
			OnClaimFreeComplete.Broadcast(false, Message);
		}
		return;
	}

	const TSharedRef<FFastGameRequestLatentState> State = Setup.State;
	FString Err;
	if (!EnsureClient(Err))
	{
		FastGameSubsystemLatent::SetLastRequest(this, 0, Err);
		OnClaimFreeComplete.Broadcast(false, Err);
		FastGameSubsystemLatent::FinishErr(State, false, Err);
		return;
	}

	const int32 Gen = ClientGeneration;
	TWeakObjectPtr<UFastGameSubsystem> WeakThis(this);
	Client->Shop->ClaimFree(GameCode, SkuKind, SkuId, [WeakThis, Gen, State](bool bOk, FString Error)
	{
		int32 Code = 0;
		FString Msg;
		FFastGameHttp::ParseStatusFromError(bOk, Error, Code, Msg);
		AsyncTask(ENamedThreads::GameThread, [WeakThis, Gen, bOk, Error, Code, Msg, State]()
		{
			if (UFastGameSubsystem* S = WeakThis.Get())
			{
				if (S->ClientGeneration == Gen)
				{
					FastGameSubsystemLatent::SetLastRequest(S, Code, Msg);
					S->OnClaimFreeComplete.Broadcast(bOk, Error);
				}
			}
			FastGameSubsystemLatent::FinishStatus(State, bOk, Code, Msg);
		});
	});
}

void UFastGameSubsystem::RedeemCode(
	const FString& GameCode,
	const FString& Code,
	FLatentActionInfo LatentInfo,
	EFastGameRequestOutcome& Outcome,
	int32& StatusCode,
	FString& Message)
{
	Outcome = EFastGameRequestOutcome::Failed;
	const FastGameSubsystemLatent::FSetup Setup = FastGameSubsystemLatent::Register(this, LatentInfo, StatusCode, Message, &Outcome);
	if (!Setup.bRegistered)
	{
		if (!Message.IsEmpty())
		{
			FastGameSubsystemLatent::SetLastRequest(this, 0, Message);
			OnClaimFreeComplete.Broadcast(false, Message);
		}
		return;
	}

	const TSharedRef<FFastGameRequestLatentState> State = Setup.State;
	FString Err;
	if (!EnsureClient(Err))
	{
		FastGameSubsystemLatent::SetLastRequest(this, 0, Err);
		OnClaimFreeComplete.Broadcast(false, Err);
		FastGameSubsystemLatent::FinishErr(State, false, Err);
		return;
	}

	const int32 Gen = ClientGeneration;
	TWeakObjectPtr<UFastGameSubsystem> WeakThis(this);
	Client->Shop->RedeemCode(GameCode, Code, [WeakThis, Gen, State](bool bOk, FString Error)
	{
		int32 Code = 0;
		FString Msg;
		FFastGameHttp::ParseStatusFromError(bOk, Error, Code, Msg);
		AsyncTask(ENamedThreads::GameThread, [WeakThis, Gen, bOk, Error, Code, Msg, State]()
		{
			if (UFastGameSubsystem* S = WeakThis.Get())
			{
				if (S->ClientGeneration == Gen)
				{
					FastGameSubsystemLatent::SetLastRequest(S, Code, Msg);
					S->OnClaimFreeComplete.Broadcast(bOk, Error);
				}
			}
			FastGameSubsystemLatent::FinishStatus(State, bOk, Code, Msg);
		});
	});
}

void UFastGameSubsystem::UnlockSku(
	const FString& GameCode,
	const FString& SkuKind,
	const FString& SkuId,
	const FString& CallbackUrl,
	const FString& DiscountCode,
	FLatentActionInfo LatentInfo,
	EFastGameShopProgress& Progress,
	int32& StatusCode,
	FString& Message,
	FFastGameBPShopUnlock& Pending)
{
	Pending = FFastGameBPShopUnlock();
	Progress = EFastGameShopProgress::Failed;

	const FastGameSubsystemLatent::FSetup Setup = FastGameSubsystemLatent::Register(this, LatentInfo, StatusCode, Message);
	if (!Setup.bRegistered)
	{
		if (!Message.IsEmpty())
		{
			FastGameSubsystemLatent::SetLastRequest(this, 0, Message);
			OnUnlockSkuComplete.Broadcast(false, false, Pending, Message);
			SetShopProgress(
				FastGameBlueprintConvert::ClassifyShopProgress(false, false, false, Message),
				false,
				Message);
			Progress = LastShopProgress;
		}
		return;
	}

	Setup.Action->ShopProgressOut = &Progress;
	Setup.Action->UnlockOut = &Pending;

	const TSharedRef<FFastGameRequestLatentState> State = Setup.State;
	FString Err;
	if (GetGameCode().IsEmpty())
	{
		Err = TEXT("FastGame: call Initialize Game first");
	}
	if (Err.IsEmpty() && !EnsureClient(Err))
	{
		// EnsureClient already set Err
	}
	if (!Err.IsEmpty())
	{
		FastGameSubsystemLatent::SetLastRequest(this, 0, Err);
		OnUnlockSkuComplete.Broadcast(false, false, Pending, Err);
		const EFastGameShopProgress Prog = FastGameBlueprintConvert::ClassifyShopProgress(false, false, false, Err);
		SetShopProgress(Prog, false, Err);
		State->ShopProgress = Prog;
		FastGameSubsystemLatent::FinishErr(State, false, Err);
		return;
	}

	bShopUnlockInFlight = true;

	const int32 Gen = ClientGeneration;
	TWeakObjectPtr<UFastGameSubsystem> WeakThis(this);
	const FString UnlockGame = GameCode;
	const FString UnlockKind = SkuKind;
	const FString UnlockId = SkuId;
	const FString UnlockCallback = CallbackUrl;
	const FString UnlockDiscount = DiscountCode;
	EnsureStoreVerifyKey([WeakThis, Gen, State, UnlockGame, UnlockKind, UnlockId, UnlockCallback, UnlockDiscount](bool /*bKeyOk*/, FString /*KeyErr*/)
	{
		UFastGameSubsystem* Host = WeakThis.Get();
		if (!Host || Host->ClientGeneration != Gen || !Host->Client.IsValid())
		{
			FastGameSubsystemLatent::FinishErr(State, false, TEXT("FastGame destroyed"));
			return;
		}
		FString SetupErr;
		if (!Host->EnsureStoreSetup(SetupErr))
		{
			Host->bShopUnlockInFlight = false;
			FastGameSubsystemLatent::SetLastRequest(Host, 0, SetupErr);
			Host->OnUnlockSkuComplete.Broadcast(false, false, FFastGameBPShopUnlock(), SetupErr);
			const EFastGameShopProgress Prog = FastGameBlueprintConvert::ClassifyShopProgress(false, false, false, SetupErr);
			Host->SetShopProgress(Prog, false, SetupErr);
			State->ShopProgress = Prog;
			FastGameSubsystemLatent::FinishErr(State, false, SetupErr);
			return;
		}
		Host->Client->Shop->UnlockSku(UnlockGame, UnlockKind, UnlockId, UnlockCallback, UnlockDiscount,
		[WeakThis, Gen, State](bool bOk, FFastGameShopUnlock Unlock, FString Error)
		{
			int32 Code = 0;
			FString Msg;
			FFastGameHttp::ParseStatusFromError(bOk, Error, Code, Msg);
			if (bOk)
			{
				Code = 200;
				Msg.Empty();
			}
			AsyncTask(ENamedThreads::GameThread, [WeakThis, Gen, bOk, Unlock, Error, Code, Msg, State]()
			{
				UFastGameSubsystem* S = WeakThis.Get();
				if (!S || S->ClientGeneration != Gen)
				{
					State->Unlock = FastGameBlueprintConvert::ToBP(Unlock);
					State->bOwned = Unlock.bOwned;
					State->ShopProgress = FastGameBlueprintConvert::ClassifyShopProgress(
						Unlock.bOwned, Unlock.bPending, bOk, Msg);
					FastGameSubsystemLatent::FinishStatus(State, bOk, Code, Msg);
					return;
				}

				const FFastGameBPShopUnlock Bp = FastGameBlueprintConvert::ToBP(Unlock);
				auto FinishUnlock = [S, State, Bp](bool bOkInner, int32 CodeInner, const FString& MsgInner, bool bOwnedInner, const FFastGameBPShopUnlock& PendingInner)
				{
					S->bShopUnlockInFlight = false;
					FastGameSubsystemLatent::SetLastRequest(S, CodeInner, MsgInner);
					S->OnUnlockSkuComplete.Broadcast(bOkInner, bOwnedInner, PendingInner, MsgInner);
					const EFastGameShopProgress Prog = FastGameBlueprintConvert::ClassifyShopProgress(
						bOwnedInner, PendingInner.bPending, bOkInner, MsgInner);
					S->SetShopProgress(Prog, bOwnedInner, MsgInner);
					State->Unlock = PendingInner;
					State->bOwned = bOwnedInner;
					State->ShopProgress = Prog;
					FastGameSubsystemLatent::FinishStatus(State, bOkInner, CodeInner, MsgInner);
				};

				if (!bOk)
				{
					FinishUnlock(false, Code, Msg.IsEmpty() ? Error : Msg, false, Bp);
					return;
				}
				if (Unlock.bOwned || !Unlock.bPending)
				{
					FinishUnlock(true, 200, TEXT(""), Unlock.bOwned, Bp);
					return;
				}

				const bool bStore = Unlock.Mode.Equals(TEXT("store"), ESearchCase::IgnoreCase)
					|| FFastGameNativeStore::IsAndroidStoreProvider(Unlock.Provider);
				if (!bStore)
				{
					FinishUnlock(true, 200, TEXT(""), false, Bp);
					return;
				}

				IFastGameNativeStore* Native = FFastGameNativeStore::Get();
				if (!Native)
				{
					FinishUnlock(false, 0, TEXT("FastGameStore plugin not loaded"), false, Bp);
					return;
				}
				if (Unlock.StoreProductId.IsEmpty())
				{
					FinishUnlock(false, 0, TEXT("StoreProductId empty — map store_skus in Fast Game"), false, Bp);
					return;
				}
				if (FastGameNeedsStoreRsa(Unlock.Provider) && S->GetStorePublicKey().IsEmpty())
				{
					FinishUnlock(
						false,
						0,
						TEXT("RSA public key is not set in Fast Game Editor payment config"),
						false,
						Bp);
					return;
				}

				Native->RequestPurchaseToken(Unlock.StoreProductId,
					[WeakThis, Gen, State, Bp, StoreProductId = Unlock.StoreProductId](FString Token, bool /*bAlready*/)
					{
						AsyncTask(ENamedThreads::GameThread, [WeakThis, Gen, State, Bp, Token, StoreProductId]()
						{
							UFastGameSubsystem* Self = WeakThis.Get();
							if (!Self || Self->ClientGeneration != Gen || !Self->Client.IsValid())
							{
								State->Unlock = Bp;
								State->ShopProgress = EFastGameShopProgress::Failed;
								FastGameSubsystemLatent::FinishStatus(State, false, 0, TEXT("FastGame destroyed"));
								return;
							}
							Self->NativeInventoryQueriedSkus.Remove(StoreProductId.TrimStartAndEnd().ToLower());
							if (Token.TrimStartAndEnd().IsEmpty())
							{
								FString EmptyMsg;
								EFastGameShopProgress Progress = EFastGameShopProgress::Cancelled;
#if PLATFORM_ANDROID
								IFastGameNativeStore* NativeStore = FFastGameNativeStore::Get();
								if (NativeStore && !NativeStore->IsStoreAppInstalled())
								{
									EmptyMsg = FastGameStoreNotInstalledMessage(Self->GetStorePlatformId());
									Progress = EFastGameShopProgress::StoreMissing;
								}
								else
								{
									EmptyMsg = TEXT("Purchase cancelled.");
								}
#else
								EmptyMsg = TEXT("Cafe Bazaar / Myket IAP only works on the Android store APK. Unreal Editor cannot return a purchase token.");
								Progress = EFastGameShopProgress::Failed;
#endif
								Self->bShopUnlockInFlight = false;
								FastGameSubsystemLatent::SetLastRequest(Self, 0, EmptyMsg);
								Self->OnUnlockSkuComplete.Broadcast(false, false, Bp, EmptyMsg);
								Self->SetShopProgress(Progress, false, EmptyMsg);
								State->Unlock = Bp;
								State->ShopProgress = Progress;
								FastGameSubsystemLatent::FinishStatus(State, false, 0, EmptyMsg);
								return;
							}
							Self->Client->Shop->CompleteUnlock(Token,
								[WeakThis, Gen, State, Bp](bool bCompOk, bool bOwnedInner, FString CompErr)
								{
									int32 CompCode = 0;
									FString CompMsg;
									if (bCompOk && bOwnedInner)
									{
										CompCode = 200;
									}
									else if (!bCompOk)
									{
										FFastGameHttp::ParseStatusFromError(bCompOk, CompErr, CompCode, CompMsg);
									}
									else
									{
										CompCode = 200;
										CompMsg = CompErr;
									}
									if (CompMsg.IsEmpty() && !bOwnedInner)
									{
										CompMsg = TEXT("Purchase validation failed");
									}
									AsyncTask(ENamedThreads::GameThread, [WeakThis, Gen, State, Bp, bCompOk, bOwnedInner, CompCode, CompMsg]()
									{
										if (UFastGameSubsystem* Done = WeakThis.Get())
										{
											if (Done->ClientGeneration == Gen)
											{
												FFastGameBPShopUnlock Out = Bp;
												Out.bOwned = bOwnedInner;
												Out.bPending = false;
												FastGameSubsystemLatent::SetLastRequest(Done, CompCode, CompMsg);
												Done->OnUnlockSkuComplete.Broadcast(bCompOk && bOwnedInner, bOwnedInner, Out, CompMsg);
												Done->bShopUnlockInFlight = false;
												Done->SetShopProgress(
													FastGameBlueprintConvert::ClassifyShopProgress(
														bOwnedInner, false, bCompOk && bOwnedInner, CompMsg),
													bOwnedInner,
													CompMsg);
												State->Unlock = Out;
												State->bOwned = bOwnedInner;
												State->ShopProgress = Done->LastShopProgress;
												FastGameSubsystemLatent::FinishStatus(State, bCompOk && bOwnedInner, CompCode, CompMsg);
												return;
											}
										}
										State->bOwned = bOwnedInner;
										State->ShopProgress = FastGameBlueprintConvert::ClassifyShopProgress(
											bOwnedInner, false, bCompOk && bOwnedInner, CompMsg);
										FastGameSubsystemLatent::FinishStatus(State, bCompOk && bOwnedInner, CompCode, CompMsg);
									});
								});
						});
					});
			});
		});
	});
}

void UFastGameSubsystem::CompleteUnlock(
	const FString& PurchaseToken,
	FLatentActionInfo LatentInfo,
	EFastGameShopProgress& Progress,
	int32& StatusCode,
	FString& Message)
{
	Progress = EFastGameShopProgress::Failed;

	const FastGameSubsystemLatent::FSetup Setup = FastGameSubsystemLatent::Register(this, LatentInfo, StatusCode, Message);
	if (!Setup.bRegistered)
	{
		if (!Message.IsEmpty())
		{
			FastGameSubsystemLatent::SetLastRequest(this, 0, Message);
			OnCompleteUnlockComplete.Broadcast(false, false, Message);
			Progress = LastShopProgress;
		}
		return;
	}

	Setup.Action->ShopProgressOut = &Progress;

	const TSharedRef<FFastGameRequestLatentState> State = Setup.State;
	FString Err;
	if (!EnsureClient(Err))
	{
		FastGameSubsystemLatent::SetLastRequest(this, 0, Err);
		OnCompleteUnlockComplete.Broadcast(false, false, Err);
		FastGameSubsystemLatent::FinishErr(State, false, Err);
		return;
	}

	const int32 Gen = ClientGeneration;
	TWeakObjectPtr<UFastGameSubsystem> WeakThis(this);
	Client->Shop->CompleteUnlock(PurchaseToken, [WeakThis, Gen, State](bool bOk, bool Owned, FString Error)
	{
		int32 Code = 0;
		FString Msg;
		FFastGameHttp::ParseStatusFromError(bOk, Error, Code, Msg);
		if (bOk && Owned)
		{
			Code = 200;
			Msg.Empty();
		}
		AsyncTask(ENamedThreads::GameThread, [WeakThis, Gen, bOk, Owned, Error, Code, Msg, State]()
		{
			if (UFastGameSubsystem* S = WeakThis.Get())
			{
				if (S->ClientGeneration == Gen)
				{
					FastGameSubsystemLatent::SetLastRequest(S, Code, Msg);
					S->OnCompleteUnlockComplete.Broadcast(bOk, Owned, Error);
					const EFastGameShopProgress Prog = FastGameBlueprintConvert::ClassifyShopProgress(
						Owned, false, bOk, Msg.IsEmpty() ? Error : Msg);
					S->SetShopProgress(Prog, Owned, Msg.IsEmpty() ? Error : Msg);
					State->ShopProgress = Prog;
				}
			}
			else
			{
				State->ShopProgress = FastGameBlueprintConvert::ClassifyShopProgress(
					Owned, false, bOk, Msg.IsEmpty() ? Error : Msg);
			}
			State->bOwned = Owned;
			FastGameSubsystemLatent::FinishStatus(State, bOk, Code, Msg);
		});
	});
}

void UFastGameSubsystem::HandleAppReactivated()
{
	if (bShopUnlockInFlight)
	{
		return;
	}
	if (!Client.IsValid() || !Client->Shop->HasPendingPayment())
	{
		return;
	}
	TWeakObjectPtr<UFastGameSubsystem> WeakThis(this);
	const int32 Gen = ClientGeneration;
	RunShopProgress([WeakThis, Gen](EFastGameShopProgress Progress, bool bOwned, FString Message)
	{
		if (UFastGameSubsystem* S = WeakThis.Get())
		{
			if (S->ClientGeneration == Gen)
			{
				S->SetShopProgress(Progress, bOwned, Message);
			}
		}
	});
}

void UFastGameSubsystem::RunShopProgress(TFunction<void(EFastGameShopProgress, bool, FString)>&& OnDone)
{
	FString SetupErr;
	if (!EnsureStoreSetup(SetupErr) && FFastGameNativeStore::IsAndroidStoreProvider(GetStorePlatformId()))
	{
		OnDone(EFastGameShopProgress::StoreMissing, false, SetupErr);
		return;
	}
	FString Err;
	if (!EnsureClient(Err))
	{
		OnDone(LastShopProgress, false, Err);
		return;
	}
	if (Client->Shop->HasPendingPayment())
	{
		TWeakObjectPtr<UFastGameSubsystem> WeakThis(this);
		const int32 Gen = ClientGeneration;
		Client->Shop->CompleteUnlock(FString(), [WeakThis, Gen, OnDone = MoveTemp(OnDone)](bool bOk, bool Owned, FString Error)
		{
			AsyncTask(ENamedThreads::GameThread, [WeakThis, Gen, bOk, Owned, Error, OnDone]()
			{
				if (!WeakThis.IsValid() || WeakThis->ClientGeneration != Gen)
				{
					OnDone(EFastGameShopProgress::Failed, Owned, Error);
					return;
				}
				const FString Msg = Error;
				EFastGameShopProgress Progress = FastGameBlueprintConvert::ClassifyShopProgress(
					Owned, !Owned && Msg.Contains(TEXT("No pending"), ESearchCase::IgnoreCase), bOk, Msg);
				if (!bOk && Msg.Contains(TEXT("No pending"), ESearchCase::IgnoreCase))
				{
					Progress = WeakThis->LastShopProgress;
				}
				OnDone(Progress, Owned, Msg);
			});
		});
		return;
	}
	OnDone(LastShopProgress, LastShopProgress == EFastGameShopProgress::Success, LastAuthMessage);
}

void UFastGameSubsystem::ShopProgress(
	FLatentActionInfo LatentInfo,
	EFastGameShopProgress& Progress,
	FString& Message)
{
	Progress = LastShopProgress;
	Message.Empty();

	int32 StatusCode = 0;
	const FastGameSubsystemLatent::FSetup Setup = FastGameSubsystemLatent::Register(this, LatentInfo, StatusCode, Message);
	if (!Setup.bRegistered)
	{
		return;
	}
	Setup.Action->ShopProgressOut = &Progress;

	const TSharedRef<FFastGameRequestLatentState> State = Setup.State;
	TWeakObjectPtr<UFastGameSubsystem> WeakThis(this);
	const int32 Gen = ClientGeneration;
	RunShopProgress([WeakThis, Gen, State](EFastGameShopProgress ProgressInner, bool bOwnedInner, FString MsgInner)
	{
		if (UFastGameSubsystem* S = WeakThis.Get())
		{
			if (S->ClientGeneration == Gen)
			{
				S->SetShopProgress(ProgressInner, bOwnedInner, MsgInner);
			}
		}
		State->ShopProgress = ProgressInner;
		State->bOwned = bOwnedInner;
		const bool bOk = ProgressInner == EFastGameShopProgress::Success
			|| ProgressInner == EFastGameShopProgress::Pending;
		FastGameSubsystemLatent::FinishStatus(State, bOk, bOk ? 200 : 0, MsgInner);
	});
}

void UFastGameSubsystem::ClearPendingPayment()
{
	if (Client.IsValid())
	{
		Client->Shop->ClearPendingPayment();
	}
}

void FFastGameAdsBlueprintHelper::RequestAd(
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
	TFunction<void(FFastGameRequestLatentAction*)> BindExtraOutputs)
{
	bHasAd = false;
	Ad = FFastGameBPAdvertisement();

	const FastGameSubsystemLatent::FSetup Setup = FastGameSubsystemLatent::RegisterWithSuccess(Self, LatentInfo, bSuccess, StatusCode, Message);
	if (!Setup.bRegistered)
	{
		if (!Message.IsEmpty())
		{
			FastGameSubsystemLatent::SetLastRequest(Self, 0, Message);
			Self->OnGetAdvertisementComplete.Broadcast(false, false, FFastGameBPAdvertisement(), Message);
		}
		return;
	}

	Setup.Action->bHasAdOut = &bHasAd;
	Setup.Action->AdOut = &Ad;
	if (BindExtraOutputs)
	{
		BindExtraOutputs(Setup.Action);
	}

	const TSharedRef<FFastGameRequestLatentState> State = Setup.State;
	FString Err;
	if (!Self->EnsureClient(Err))
	{
		FastGameSubsystemLatent::SetLastRequest(Self, 0, Err);
		Self->OnGetAdvertisementComplete.Broadcast(false, false, FFastGameBPAdvertisement(), Err);
		FastGameSubsystemLatent::FinishErr(State, false, Err);
		return;
	}

	FFastGameAdvertisementRequest Req;
	Req.GameId = GameId;
	Req.Slot = Slot;
	Req.MediaType = MediaType;
	Req.Format = Format;
	Req.Tags = Tags;
	Req.Locale = Locale;
	Req.Country = Country;
	Req.Platform = Platform;
	Req.Engine = Engine;
	if (!MediaType.IsEmpty())
	{
		TSharedPtr<FJsonObject> Caps = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> Types;
		Types.Add(MakeShared<FJsonValueString>(MediaType));
		Caps->SetArrayField(TEXT("mediaTypes"), Types);
		Req.Capabilities = Caps;
	}

	const int32 Gen = Self->ClientGeneration;
	TWeakObjectPtr<UFastGameSubsystem> WeakThis(Self);
	Self->Client->Ads->GetAdvertisement(Req,
		[WeakThis, Gen, State](bool bOk, bool bFilled, FFastGameAdvertisement NativeAd, FString Error)
		{
			const FFastGameBPAdvertisement BpAd = bOk && bFilled
				? FastGameBlueprintConvert::ToBP(NativeAd)
				: FFastGameBPAdvertisement();
			int32 Code = 0;
			FString Msg;
			FFastGameHttp::ParseStatusFromError(bOk, Error, Code, Msg);
			if (bOk && !bFilled) Code = 204;
			AsyncTask(ENamedThreads::GameThread, [WeakThis, Gen, bOk, bFilled, BpAd, Error, Code, Msg, State]()
			{
				if (UFastGameSubsystem* S = WeakThis.Get())
				{
					if (S->ClientGeneration == Gen)
					{
						FastGameSubsystemLatent::SetLastRequest(S, Code, Msg);
						S->OnGetAdvertisementComplete.Broadcast(bOk, bFilled, BpAd, Error);
					}
				}
				State->Ad = BpAd;
				State->bHasAd = bFilled;
				FastGameSubsystemLatent::FinishStatus(State, bOk, Code, Msg);
			});
		});
}

void FFastGameAdsBlueprintHelper::Track(
	UFastGameSubsystem* Self,
	const FString& EventType,
	const FString& AdId,
	const FString& GameId,
	const FString& CampaignId,
	FLatentActionInfo LatentInfo,
	bool& bSuccess,
	int32& StatusCode,
	FString& Message)
{
	const FastGameSubsystemLatent::FSetup Setup = FastGameSubsystemLatent::RegisterWithSuccess(Self, LatentInfo, bSuccess, StatusCode, Message);
	if (!Setup.bRegistered)
	{
		if (!Message.IsEmpty())
		{
			FastGameSubsystemLatent::SetLastRequest(Self, 0, Message);
			Self->OnTrackAdEventComplete.Broadcast(false, Message);
		}
		return;
	}

	const TSharedRef<FFastGameRequestLatentState> State = Setup.State;
	FString Err;
	if (!Self->EnsureClient(Err))
	{
		FastGameSubsystemLatent::SetLastRequest(Self, 0, Err);
		Self->OnTrackAdEventComplete.Broadcast(false, Err);
		FastGameSubsystemLatent::FinishErr(State, false, Err);
		return;
	}

	FFastGameAdvertisementEvent Evt;
	Evt.EventType = EventType;
	Evt.AdId = AdId;
	Evt.GameId = GameId;
	Evt.CampaignId = CampaignId;

	const int32 Gen = Self->ClientGeneration;
	TWeakObjectPtr<UFastGameSubsystem> WeakThis(Self);
	Self->Client->Ads->TrackEvent(Evt,
		[WeakThis, Gen, State](bool bOk, FString Error)
		{
			int32 Code = 0;
			FString Msg;
			FFastGameHttp::ParseStatusFromError(bOk, Error, Code, Msg);
			if (bOk) Code = 204;
			AsyncTask(ENamedThreads::GameThread, [WeakThis, Gen, bOk, Error, Code, Msg, State]()
			{
				if (UFastGameSubsystem* S = WeakThis.Get())
				{
					if (S->ClientGeneration == Gen)
					{
						FastGameSubsystemLatent::SetLastRequest(S, Code, Msg);
						S->OnTrackAdEventComplete.Broadcast(bOk, Error);
					}
				}
				FastGameSubsystemLatent::FinishStatus(State, bOk, Code, Msg);
			});
		});
}

void UFastGameSubsystem::GetAdvertisement(
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
	FFastGameBPAdvertisement& Ad)
{
	FFastGameAdsBlueprintHelper::RequestAd(
		this, GameId, Slot, MediaType, Format, Tags, Locale, Country, Platform, Engine,
		LatentInfo, bSuccess, StatusCode, Message, bHasAd, Ad, nullptr);
}

void UFastGameSubsystem::GetImageAd(
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
	FFastGameBPAdvertisement& Ad)
{
	ImageUrl.Reset();
	ClickUrl.Reset();
	Width = 0;
	Height = 0;
	FFastGameAdsBlueprintHelper::RequestAd(
		this, GameId, Slot, TEXT("image"), Format, Tags, Locale, Country, Platform, Engine,
		LatentInfo, bSuccess, StatusCode, Message, bHasAd, Ad,
		[&](FFastGameRequestLatentAction* Action)
		{
			Action->ImageUrlOut = &ImageUrl;
			Action->ClickUrlOut = &ClickUrl;
			Action->MediaWidthOut = &Width;
			Action->MediaHeightOut = &Height;
		});
}

void UFastGameSubsystem::GetVideoAd(
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
	FFastGameBPAdvertisement& Ad)
{
	VideoUrl.Reset();
	ClickUrl.Reset();
	FFastGameAdsBlueprintHelper::RequestAd(
		this, GameId, Slot, TEXT("video"), Format, Tags, Locale, Country, Platform, Engine,
		LatentInfo, bSuccess, StatusCode, Message, bHasAd, Ad,
		[&](FFastGameRequestLatentAction* Action)
		{
			Action->VideoUrlOut = &VideoUrl;
			Action->ClickUrlOut = &ClickUrl;
		});
}

void UFastGameSubsystem::GetGifAd(
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
	FFastGameBPAdvertisement& Ad)
{
	MediaUrl.Reset();
	ClickUrl.Reset();
	FFastGameAdsBlueprintHelper::RequestAd(
		this, GameId, Slot, TEXT("gif"), Format, Tags, Locale, Country, Platform, Engine,
		LatentInfo, bSuccess, StatusCode, Message, bHasAd, Ad,
		[&](FFastGameRequestLatentAction* Action)
		{
			Action->MediaUrlOut = &MediaUrl;
			Action->ClickUrlOut = &ClickUrl;
		});
}

void UFastGameSubsystem::GetLottieAd(
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
	FFastGameBPAdvertisement& Ad)
{
	MediaUrl.Reset();
	ClickUrl.Reset();
	FFastGameAdsBlueprintHelper::RequestAd(
		this, GameId, Slot, TEXT("lottie"), Format, Tags, Locale, Country, Platform, Engine,
		LatentInfo, bSuccess, StatusCode, Message, bHasAd, Ad,
		[&](FFastGameRequestLatentAction* Action)
		{
			Action->MediaUrlOut = &MediaUrl;
			Action->ClickUrlOut = &ClickUrl;
		});
}

void UFastGameSubsystem::GetRiveAd(
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
	FFastGameBPAdvertisement& Ad)
{
	MediaUrl.Reset();
	ClickUrl.Reset();
	FFastGameAdsBlueprintHelper::RequestAd(
		this, GameId, Slot, TEXT("rive"), Format, Tags, Locale, Country, Platform, Engine,
		LatentInfo, bSuccess, StatusCode, Message, bHasAd, Ad,
		[&](FFastGameRequestLatentAction* Action)
		{
			Action->MediaUrlOut = &MediaUrl;
			Action->ClickUrlOut = &ClickUrl;
		});
}

void UFastGameSubsystem::GetTextAd(
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
	FFastGameBPAdvertisement& Ad)
{
	Title.Reset();
	Body.Reset();
	BackgroundUrl.Reset();
	BackgroundColor.Reset();
	ClickUrl.Reset();
	FFastGameAdsBlueprintHelper::RequestAd(
		this, GameId, Slot, TEXT("text"), Format, Tags, Locale, Country, Platform, Engine,
		LatentInfo, bSuccess, StatusCode, Message, bHasAd, Ad,
		[&](FFastGameRequestLatentAction* Action)
		{
			Action->TitleOut = &Title;
			Action->BodyOut = &Body;
			Action->BackgroundUrlOut = &BackgroundUrl;
			Action->BackgroundColorOut = &BackgroundColor;
			Action->ClickUrlOut = &ClickUrl;
		});
}

void UFastGameSubsystem::TrackAdEvent(
	const FString& EventType,
	const FString& AdId,
	const FString& GameId,
	const FString& CampaignId,
	FLatentActionInfo LatentInfo,
	bool& bSuccess,
	int32& StatusCode,
	FString& Message)
{
	FFastGameAdsBlueprintHelper::Track(this, EventType, AdId, GameId, CampaignId, LatentInfo, bSuccess, StatusCode, Message);
}

void UFastGameSubsystem::TrackAdDisplayed(
	const FFastGameBPAdvertisement& Ad,
	const FString& GameId,
	FLatentActionInfo LatentInfo,
	bool& bSuccess,
	int32& StatusCode,
	FString& Message)
{
	FFastGameAdsBlueprintHelper::Track(
		this, TEXT("AdvertisementDisplayed"), Ad.Id, GameId, Ad.CampaignId,
		LatentInfo, bSuccess, StatusCode, Message);
}

void UFastGameSubsystem::TrackAdClicked(
	const FFastGameBPAdvertisement& Ad,
	const FString& GameId,
	FLatentActionInfo LatentInfo,
	bool& bSuccess,
	int32& StatusCode,
	FString& Message)
{
	FFastGameAdsBlueprintHelper::Track(
		this, TEXT("AdvertisementClicked"), Ad.Id, GameId, Ad.CampaignId,
		LatentInfo, bSuccess, StatusCode, Message);
}

void UFastGameSubsystem::TrackAdClosed(
	const FFastGameBPAdvertisement& Ad,
	const FString& GameId,
	FLatentActionInfo LatentInfo,
	bool& bSuccess,
	int32& StatusCode,
	FString& Message)
{
	FFastGameAdsBlueprintHelper::Track(
		this, TEXT("AdvertisementClosed"), Ad.Id, GameId, Ad.CampaignId,
		LatentInfo, bSuccess, StatusCode, Message);
}

bool UFastGameSubsystem::IsImageAd(const FFastGameBPAdvertisement& Ad) const
{
	return Ad.MediaType.Equals(TEXT("image"), ESearchCase::IgnoreCase);
}

bool UFastGameSubsystem::IsVideoAd(const FFastGameBPAdvertisement& Ad) const
{
	return Ad.MediaType.Equals(TEXT("video"), ESearchCase::IgnoreCase);
}

bool UFastGameSubsystem::IsGifAd(const FFastGameBPAdvertisement& Ad) const
{
	return Ad.MediaType.Equals(TEXT("gif"), ESearchCase::IgnoreCase);
}

bool UFastGameSubsystem::IsLottieAd(const FFastGameBPAdvertisement& Ad) const
{
	return Ad.MediaType.Equals(TEXT("lottie"), ESearchCase::IgnoreCase);
}

bool UFastGameSubsystem::IsRiveAd(const FFastGameBPAdvertisement& Ad) const
{
	return Ad.MediaType.Equals(TEXT("rive"), ESearchCase::IgnoreCase);
}

bool UFastGameSubsystem::IsTextAd(const FFastGameBPAdvertisement& Ad) const
{
	return Ad.MediaType.Equals(TEXT("text"), ESearchCase::IgnoreCase);
}

void UFastGameSubsystem::BreakAdvertisement(
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
	FString& BackgroundColor)
{
	Id = Ad.Id;
	CampaignId = Ad.CampaignId;
	MediaType = Ad.MediaType;
	MediaUrl = Ad.MediaUrl;
	Width = Ad.MediaWidth;
	Height = Ad.MediaHeight;
	bClickEnabled = Ad.bClickEnabled;
	ClickUrl = Ad.ClickUrl;
	Title = Ad.Title;
	Body = Ad.Body;
	BackgroundUrl = Ad.BackgroundUrl;
	BackgroundColor = Ad.BackgroundColor;
}

TArray<FFastGameBPAssetPack> UFastGameSubsystem::ListPacksFromGameDetail(const FFastGameBPCatalogDetail& Detail) const
{
	return Detail.AssetPacks;
}
