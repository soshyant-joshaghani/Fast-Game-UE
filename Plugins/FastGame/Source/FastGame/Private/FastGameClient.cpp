#include "FastGameClient.h"
#include "FastGame.h"
#include "FastGameNativeStore.h"
#include "FastGameStoreVerify.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HAL/PlatformProcess.h"

namespace FastGameJsonUtil
{
	static TSharedPtr<FJsonObject> ParseObject(const FString& Text)
	{
		TSharedPtr<FJsonValue> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid() || Root->Type != EJson::Object)
		{
			return nullptr;
		}
		return Root->AsObject();
	}

	static TArray<TSharedPtr<FJsonValue>> ParseArray(const FString& Text)
	{
		TSharedPtr<FJsonValue> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		TArray<TSharedPtr<FJsonValue>> Out;
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid() || Root->Type != EJson::Array)
		{
			return Out;
		}
		return Root->AsArray();
	}

	static FString Stringify(const TSharedPtr<FJsonObject>& Obj)
	{
		FString Out;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
		return Out;
	}

	static FString Escape(const FString& S)
	{
		return FGenericPlatformHttp::UrlEncode(S);
	}

	static void ParseStringArray(const TSharedPtr<FJsonObject>& O, const FString& Key, TArray<FString>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (!O.IsValid() || !O->TryGetArrayField(Key, Arr) || !Arr)
		{
			return;
		}
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const FString S = V->AsString().TrimStartAndEnd();
			if (!S.IsEmpty())
			{
				Out.Add(S);
			}
		}
	}

	static FFastGameAssetPack ParseAssetPack(const TSharedPtr<FJsonObject>& P)
	{
		FFastGameAssetPack Pack;
		if (!P.IsValid())
		{
			return Pack;
		}
		P->TryGetStringField(TEXT("id"), Pack.Id);
		P->TryGetStringField(TEXT("pack_id"), Pack.PackId);
		P->TryGetStringField(TEXT("label"), Pack.Label);
		double Rev = 0;
		P->TryGetNumberField(TEXT("revision"), Rev);
		Pack.Revision = static_cast<int32>(Rev);
		P->TryGetStringField(TEXT("version"), Pack.Version);
		P->TryGetStringField(TEXT("url"), Pack.Url);
		P->TryGetStringField(TEXT("hash"), Pack.Hash);
		ParseStringArray(P, TEXT("quality"), Pack.Quality);
		ParseStringArray(P, TEXT("platforms"), Pack.Platforms);
		ParseStringArray(P, TEXT("languages"), Pack.Languages);
		P->TryGetStringField(TEXT("kind"), Pack.Kind);
		if (Pack.Kind.IsEmpty())
		{
			Pack.Kind = TEXT("content");
		}
		return Pack;
	}

	static FFastGameCatalogEntry ParseCatalog(const TSharedPtr<FJsonObject>& O)
	{
		FFastGameCatalogEntry E;
		if (!O.IsValid()) return E;
		O->TryGetStringField(TEXT("id"), E.Id);
		O->TryGetStringField(TEXT("game_id"), E.GameId);
		O->TryGetStringField(TEXT("label"), E.Label);
		O->TryGetStringField(TEXT("description"), E.Description);
		O->TryGetStringField(TEXT("colyseus_room"), E.ColyseusRoom);
		O->TryGetBoolField(TEXT("available"), E.bAvailable);
		const TSharedPtr<FJsonObject>* Translations = nullptr;
		if (O->TryGetObjectField(TEXT("translations"), Translations) && Translations)
			E.Translations = *Translations;
		return E;
	}

	static FFastGameCatalogDetail ParseCatalogDetail(const TSharedPtr<FJsonObject>& O)
	{
		FFastGameCatalogDetail D;
		const FFastGameCatalogEntry Base = ParseCatalog(O);
		D.Id = Base.Id;
		D.GameId = Base.GameId;
		D.Label = Base.Label;
		D.Description = Base.Description;
		D.ColyseusRoom = Base.ColyseusRoom;
		D.bAvailable = Base.bAvailable;
		D.Translations = Base.Translations;
		if (!O.IsValid()) return D;

		const TSharedPtr<FJsonObject>* AuthReq = nullptr;
		if (O->TryGetObjectField(TEXT("auth_requirements"), AuthReq) && AuthReq && AuthReq->IsValid())
		{
			(*AuthReq)->TryGetBoolField(TEXT("verify_phone"), D.bAuthVerifyPhone);
			(*AuthReq)->TryGetBoolField(TEXT("verify_email"), D.bAuthVerifyEmail);
		}

		const TArray<TSharedPtr<FJsonValue>>* Modes = nullptr;
		if (O->TryGetArrayField(TEXT("modes"), Modes) && Modes)
		{
			for (const auto& V : *Modes)
			{
				const TSharedPtr<FJsonObject> M = V->AsObject();
				if (!M.IsValid()) continue;
				FFastGameMode Mode;
				M->TryGetStringField(TEXT("id"), Mode.Id);
				M->TryGetStringField(TEXT("mode_id"), Mode.ModeId);
				M->TryGetStringField(TEXT("topology"), Mode.Topology);
				M->TryGetStringField(TEXT("win_kind"), Mode.WinKind);
				double MinP = 0, MaxP = 0;
				M->TryGetNumberField(TEXT("min_players"), MinP);
				M->TryGetNumberField(TEXT("max_players"), MaxP);
				Mode.MinPlayers = static_cast<int32>(MinP);
				Mode.MaxPlayers = static_cast<int32>(MaxP);
				M->TryGetStringField(TEXT("kind"), Mode.Kind);
				D.Modes.Add(Mode);
			}
		}
		const TArray<TSharedPtr<FJsonValue>>* Maps = nullptr;
		if (O->TryGetArrayField(TEXT("maps"), Maps) && Maps)
		{
			for (const auto& V : *Maps)
			{
				const TSharedPtr<FJsonObject> M = V->AsObject();
				if (!M.IsValid()) continue;
				FFastGameMap Map;
				M->TryGetStringField(TEXT("id"), Map.Id);
				M->TryGetStringField(TEXT("map_id"), Map.MapId);
				M->TryGetStringField(TEXT("label"), Map.Label);
				M->TryGetBoolField(TEXT("purchasable"), Map.bPurchasable);
				double Price = 0;
				M->TryGetNumberField(TEXT("price"), Price);
				Map.Price = static_cast<int32>(Price);
				const TSharedPtr<FJsonObject>* MapTr = nullptr;
				if (M->TryGetObjectField(TEXT("translations"), MapTr) && MapTr)
					Map.Translations = *MapTr;
				const TArray<TSharedPtr<FJsonValue>>* SM = nullptr;
				if (M->TryGetArrayField(TEXT("supported_modes"), SM) && SM)
				{
					for (const auto& S : *SM) Map.SupportedModes.Add(S->AsString());
				}
				D.Maps.Add(Map);
			}
		}
		const TArray<TSharedPtr<FJsonValue>>* Packs = nullptr;
		if (O->TryGetArrayField(TEXT("asset_packs"), Packs) && Packs)
		{
			for (const auto& V : *Packs)
			{
				D.AssetPacks.Add(ParseAssetPack(V->AsObject()));
			}
		}
		return D;
	}

	static FFastGameShopLine ParseShopLine(const TSharedPtr<FJsonObject>& O)
	{
		FFastGameShopLine L;
		if (!O.IsValid()) return L;
		O->TryGetStringField(TEXT("game_code"), L.GameCode);
		O->TryGetStringField(TEXT("sku_kind"), L.SkuKind);
		O->TryGetStringField(TEXT("sku_id"), L.SkuId);
		O->TryGetStringField(TEXT("label"), L.Label);
		double Price = 0;
		O->TryGetNumberField(TEXT("price"), Price);
		L.Price = static_cast<int32>(Price);
		O->TryGetBoolField(TEXT("owned"), L.bOwned);
		const TSharedPtr<FJsonObject>* Meta = nullptr;
		if (O->TryGetObjectField(TEXT("meta"), Meta) && Meta)
		{
			L.Meta = *Meta;
		}
		return L;
	}

	static FFastGameLoadout ParseLoadout(const TSharedPtr<FJsonObject>& O)
	{
		FFastGameLoadout L;
		if (!O.IsValid()) return L;
		O->TryGetStringField(TEXT("user_id"), L.UserId);
		O->TryGetStringField(TEXT("game_code"), L.GameCode);
		O->TryGetStringField(TEXT("character_id"), L.CharacterId);
		double Level = 0, Xp = 0;
		O->TryGetNumberField(TEXT("level"), Level);
		O->TryGetNumberField(TEXT("xp"), Xp);
		L.Level = static_cast<int32>(Level);
		L.Xp = static_cast<int32>(Xp);
		const TSharedPtr<FJsonObject>* Cos = nullptr;
		if (O->TryGetObjectField(TEXT("equipped_cosmetics"), Cos) && Cos)
		{
			for (const auto& Kv : (*Cos)->Values)
			{
				L.EquippedCosmetics.Add(Kv.Key, Kv.Value->AsString());
			}
		}
		const TSharedPtr<FJsonObject>* Parts = nullptr;
		if (O->TryGetObjectField(TEXT("modular_parts"), Parts) && Parts)
		{
			for (const auto& Kv : (*Parts)->Values)
			{
				L.ModularParts.Add(Kv.Key, Kv.Value->AsString());
			}
		}
		return L;
	}

	static FString PendingPath(const FString& Slot)
	{
		return FPaths::ProjectSavedDir() / TEXT("FastGame") / (Slot + TEXT(".json"));
	}

	static FString AccessTokenPath(const FString& Slot)
	{
		return FPaths::ProjectSavedDir() / TEXT("FastGame") / (Slot + TEXT(".txt"));
	}

	static void SaveAccessTokenFile(const FString& Slot, const FString& Token)
	{
		if (Slot.IsEmpty())
		{
			return;
		}
		const FString Path = AccessTokenPath(Slot);
		if (Token.IsEmpty())
		{
			IFileManager::Get().Delete(*Path);
			return;
		}
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
		FFileHelper::SaveStringToFile(Token, *Path);
	}

	static void DeleteAccessTokenFile(const FString& Slot)
	{
		if (!Slot.IsEmpty())
		{
			IFileManager::Get().Delete(*AccessTokenPath(Slot));
		}
	}

	static FString LoadAccessTokenFile(const FString& Slot)
	{
		if (Slot.IsEmpty())
		{
			return FString();
		}
		FString Token;
		if (FFileHelper::LoadFileToString(Token, *AccessTokenPath(Slot)))
		{
			Token.TrimStartAndEndInline();
		}
		return Token;
	}

	/** Two-line file: channel (email|phone) then identity. */
	static void SaveEnteredIdentityFile(const FString& Slot, const FString& Channel, const FString& Identity)
	{
		if (Slot.IsEmpty())
		{
			return;
		}
		const FString Path = AccessTokenPath(Slot);
		if (Identity.IsEmpty())
		{
			IFileManager::Get().Delete(*Path);
			return;
		}
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
		FFileHelper::SaveStringToFile(Channel + TEXT("\n") + Identity, *Path);
	}

	static void DeleteEnteredIdentityFile(const FString& Slot)
	{
		if (!Slot.IsEmpty())
		{
			IFileManager::Get().Delete(*AccessTokenPath(Slot));
		}
	}

	static bool LoadEnteredIdentityFile(const FString& Slot, FString& OutChannel, FString& OutIdentity)
	{
		OutChannel.Reset();
		OutIdentity.Reset();
		if (Slot.IsEmpty())
		{
			return false;
		}
		FString Raw;
		if (!FFileHelper::LoadFileToString(Raw, *AccessTokenPath(Slot)))
		{
			return false;
		}
		Raw.TrimStartAndEndInline();
		FString Left, Right;
		if (Raw.Split(TEXT("\n"), &Left, &Right))
		{
			OutChannel = Left.TrimStartAndEnd();
			OutIdentity = Right.TrimStartAndEnd();
		}
		else
		{
			OutIdentity = Raw;
			OutChannel = TEXT("email");
		}
		return !OutIdentity.IsEmpty();
	}
}

FString FastGameNormalizeProviderId(const FString& Provider)
{
	FString Id = Provider;
	Id.TrimStartAndEndInline();
	Id.ToLowerInline();
	if (Id == TEXT("cafebazaar") || Id == TEXT("cafe_bazaar") || Id == TEXT("bazaar")
		|| Id == TEXT("cafe-bazaar"))
	{
		return TEXT("caffebazar");
	}
	if (Id == TEXT("google_play") || Id == TEXT("play") || Id == TEXT("google-play"))
	{
		return TEXT("googleplay");
	}
	if (Id == TEXT("app_store") || Id == TEXT("ios") || Id == TEXT("apple"))
	{
		return TEXT("appstore");
	}
	return Id;
}

FString FastGameStoreDisplayName(const FString& Provider)
{
	const FString Id = FastGameNormalizeProviderId(Provider);
	if (Id == TEXT("caffebazar"))
	{
		return TEXT("Cafe Bazaar");
	}
	if (Id == TEXT("myket"))
	{
		return TEXT("Myket");
	}
	if (Id == TEXT("googleplay"))
	{
		return TEXT("Google Play");
	}
	if (Id.IsEmpty())
	{
		return TEXT("the store app");
	}
	return Id;
}

FString FastGameStoreNotInstalledMessage(const FString& Provider)
{
	const FString Name = FastGameStoreDisplayName(Provider);
	return FString::Printf(
		TEXT("%s is not installed on this device. Install %s, then open the game again."),
		*Name,
		*Name);
}

FFastGameClient::FFastGameClient(FFastGameConfig InConfig)
	: Config(InConfig)
	, Http(MakeShared<FFastGameHttp>(Config.ApiBaseUrl))
	, Auth(MakeShared<FFastGameAuth>(Http, Config))
	, Catalog(MakeShared<FFastGameCatalog>(Http))
	, Content(MakeShared<FFastGameContent>(Http, Catalog))
	, Realtime(MakeShared<FFastGameRealtime>(Http))
	, Progress(MakeShared<FFastGameProgress>(Http))
	, Shop(MakeShared<FFastGameShop>(Http, Config))
	, Ads(MakeShared<FFastGameAds>(Http))
{
	TSharedRef<FFastGameShop> ShopRef = Shop;
	Auth->OnLoggedIn = [ShopRef]()
	{
		ShopRef->BindStoreLock();
	};
	if (Auth->IsLoggedIn())
	{
		Shop->BindStoreLock();
	}
}

void FFastGameAuth::SetAccessToken(const FString& Token)
{
	Http->SetAccessToken(Token);
	PersistAccessToken(Token);
	if (!Token.IsEmpty() && OnLoggedIn)
	{
		OnLoggedIn();
	}
}

void FFastGameAuth::Logout()
{
	Http->SetAccessToken(TEXT(""));
	DeletePersistedAccessToken();
}

void FFastGameAuth::ClearLocalCache()
{
	Logout();
	ClearEnteredIdentity();
	if (!Config.PendingPaymentSaveSlot.IsEmpty())
	{
		IFileManager::Get().Delete(*FastGameJsonUtil::PendingPath(Config.PendingPaymentSaveSlot));
	}
}

void FFastGameAuth::SetGameCode(const FString& InGameCode)
{
	Config.GameCode = InGameCode.TrimStartAndEnd();
}

bool FFastGameAuth::RequireGameCode(FString& OutGameCode, FString& OutError) const
{
	OutGameCode = Config.GameCode.TrimStartAndEnd();
	if (OutGameCode.IsEmpty())
	{
		OutError = TEXT("FastGame: GameCode not set — call Initialize Game");
		return false;
	}
	OutError.Reset();
	return true;
}

void FFastGameAuth::StoreEnteredIdentity(const FString& Identity, bool bIsEmail)
{
	EnteredIdentity = Identity.TrimStartAndEnd();
	EnteredChannel = bIsEmail ? EFastGameIdentityChannel::Email : EFastGameIdentityChannel::Phone;
	PersistEnteredIdentity();
}

void FFastGameAuth::ClearEnteredIdentity()
{
	EnteredIdentity.Reset();
	EnteredChannel = EFastGameIdentityChannel::Auto;
	DeletePersistedEnteredIdentity();
}

bool FFastGameAuth::HasEnteredIdentity() const
{
	if (!EnteredIdentity.IsEmpty())
	{
		return true;
	}
	FString ChannelStr, Identity;
	return FastGameJsonUtil::LoadEnteredIdentityFile(Config.EnteredIdentitySaveSlot, ChannelStr, Identity)
		&& !Identity.IsEmpty();
}

bool FFastGameAuth::EnsureEnteredIdentityLoaded()
{
	if (!EnteredIdentity.IsEmpty())
	{
		return true;
	}
	LoadPersistedEnteredIdentity();
	return !EnteredIdentity.IsEmpty();
}

void FFastGameAuth::LoadPersistedEnteredIdentity()
{
	FString ChannelStr, Identity;
	if (!FastGameJsonUtil::LoadEnteredIdentityFile(Config.EnteredIdentitySaveSlot, ChannelStr, Identity))
	{
		return;
	}
	EnteredIdentity = Identity;
	EnteredChannel = ChannelStr.Equals(TEXT("phone"), ESearchCase::IgnoreCase)
		? EFastGameIdentityChannel::Phone
		: EFastGameIdentityChannel::Email;
}

void FFastGameAuth::PersistEnteredIdentity() const
{
	const FString ChannelStr = (EnteredChannel == EFastGameIdentityChannel::Phone)
		? TEXT("phone")
		: TEXT("email");
	FastGameJsonUtil::SaveEnteredIdentityFile(Config.EnteredIdentitySaveSlot, ChannelStr, EnteredIdentity);
}

void FFastGameAuth::DeletePersistedEnteredIdentity() const
{
	FastGameJsonUtil::DeleteEnteredIdentityFile(Config.EnteredIdentitySaveSlot);
}

void FFastGameAuth::LoadPersistedAccessToken()
{
	const FString Token = FastGameJsonUtil::LoadAccessTokenFile(Config.AccessTokenSaveSlot);
	if (!Token.IsEmpty())
	{
		Http->SetAccessToken(Token);
	}
}

void FFastGameAuth::PersistAccessToken(const FString& Token) const
{
	FastGameJsonUtil::SaveAccessTokenFile(Config.AccessTokenSaveSlot, Token);
}

void FFastGameAuth::DeletePersistedAccessToken() const
{
	FastGameJsonUtil::DeleteAccessTokenFile(Config.AccessTokenSaveSlot);
}

bool FFastGameAuth::FillEmailPhoneFromEntered(FString& InOutEmail, FString& InOutPhone, FString& OutError)
{
	if (!InOutEmail.IsEmpty() || !InOutPhone.IsEmpty())
	{
		OutError.Reset();
		return true;
	}
	if (!EnsureEnteredIdentityLoaded())
	{
		OutError = TEXT("No contact provided and no ENTER-stored identity");
		return false;
	}
	if (EnteredChannel == EFastGameIdentityChannel::Phone)
	{
		InOutPhone = EnteredIdentity;
	}
	else if (EnteredChannel == EFastGameIdentityChannel::Email)
	{
		InOutEmail = EnteredIdentity;
	}
	else if (FastGameIdentity::LooksLikeEmail(EnteredIdentity))
	{
		InOutEmail = EnteredIdentity;
	}
	else if (FastGameIdentity::LooksLikePhone(EnteredIdentity))
	{
		InOutPhone = EnteredIdentity;
	}
	else
	{
		OutError = TEXT("ENTER-stored identity is not a valid email or phone");
		return false;
	}
	OutError.Reset();
	return true;
}

bool FFastGameAuth::ResolveContactFields(const FString& Identity, const FString& Email, const FString& Phone,
	FString& OutEmail, FString& OutPhone, FString& OutError)
{
	OutEmail = Email.TrimStartAndEnd();
	OutPhone = Phone.TrimStartAndEnd();
	FString EffectiveIdentity = Identity.TrimStartAndEnd();
	if (OutEmail.IsEmpty() && OutPhone.IsEmpty() && EffectiveIdentity.IsEmpty())
	{
		if (!FillEmailPhoneFromEntered(OutEmail, OutPhone, OutError))
		{
			return false;
		}
		return true;
	}
	if (OutEmail.IsEmpty() && OutPhone.IsEmpty())
	{
		if (!FastGameIdentity::TrySplitContact(EffectiveIdentity, OutEmail, OutPhone))
		{
			OutError = TEXT("Provide a valid email or phone number");
			return false;
		}
	}
	else
	{
		if (!OutEmail.IsEmpty() && !FastGameIdentity::LooksLikeEmail(OutEmail))
		{
			OutError = TEXT("Invalid email format");
			return false;
		}
		if (!OutPhone.IsEmpty() && !FastGameIdentity::LooksLikePhone(OutPhone))
		{
			OutError = TEXT("Invalid phone format");
			return false;
		}
	}
	OutError.Reset();
	return true;
}

void FFastGameAuth::PostContactJson(const FString& Path, const FString& Email, const FString& Phone,
	TFunction<void(TSharedPtr<FJsonObject>)> AugmentBody,
	TFunction<void(bool, int32, FString)> OnDone)
{
	FString GameCode, GameErr;
	if (!RequireGameCode(GameCode, GameErr))
	{
		if (OnDone) OnDone(false, 0, GameErr);
		return;
	}
	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("game_code"), GameCode);
	if (!Email.IsEmpty())
	{
		Body->SetStringField(TEXT("email"), Email);
	}
	if (!Phone.IsEmpty())
	{
		Body->SetStringField(TEXT("phone"), Phone);
	}
	if (AugmentBody)
	{
		AugmentBody(Body);
	}
	Http->PostJson(Path, FastGameJsonUtil::Stringify(Body),
		[OnDone](bool bOk, int32 StatusCode, FString Resp, FString Err)
		{
			if (!bOk)
			{
				if (OnDone) OnDone(false, StatusCode, FFastGameHttp::ExtractApiMessage(StatusCode, Resp, Err));
				return;
			}
			if (OnDone) OnDone(true, StatusCode, TEXT(""));
		});
}

void FFastGameAuth::Login(const FString& Identity, const FString& Password,
	TFunction<void(bool, int32, FString, FString)> OnDone, EFastGameIdentityChannel Channel)
{
	if (Password.IsEmpty())
	{
		if (OnDone) OnDone(false, 0, TEXT(""), TEXT("Email/phone and password are required"));
		return;
	}
	FString EffectiveIdentity = Identity.TrimStartAndEnd();
	EFastGameIdentityChannel EffectiveChannel = Channel;
	if (EffectiveIdentity.IsEmpty())
	{
		if (!EnsureEnteredIdentityLoaded())
		{
			if (OnDone) OnDone(false, 0, TEXT(""), TEXT("No Identity provided and no ENTER-stored identity"));
			return;
		}
		EffectiveIdentity = EnteredIdentity;
		if (EffectiveChannel == EFastGameIdentityChannel::Auto)
		{
			EffectiveChannel = EnteredChannel;
		}
	}
	FString OutEmail, OutPhone, Err;
	if (!FastGameIdentity::ResolveChannel(EffectiveIdentity, EffectiveChannel, OutEmail, OutPhone, Err))
	{
		if (OnDone) OnDone(false, 0, TEXT(""), Err);
		return;
	}
	if (!OutEmail.IsEmpty())
	{
		LoginWithEmail(OutEmail, Password, OnDone);
		return;
	}
	LoginWithPhone(OutPhone, Password, OnDone);
}

void FFastGameAuth::Enter(const FString& Identity, EFastGameIdentityChannel Channel,
	TFunction<void(bool, int32, bool, bool, FString, FString, FString, FString)> OnDone)
{
	FString OutEmail, OutPhone, Err;
	if (!FastGameIdentity::ResolveChannel(Identity, Channel, OutEmail, OutPhone, Err))
	{
		if (OnDone) OnDone(false, 0, false, false, TEXT(""), TEXT(""), TEXT(""), Err);
		return;
	}

	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	if (!OutEmail.IsEmpty())
	{
		Body->SetStringField(TEXT("email"), OutEmail);
	}
	else
	{
		Body->SetStringField(TEXT("phone"), OutPhone);
	}

	Http->PostJson(TEXT("/base/login/enter"), FastGameJsonUtil::Stringify(Body),
		[this, OnDone](bool bOk, int32 StatusCode, FString Resp, FString Err)
		{
			if (!bOk)
			{
				if (OnDone)
				{
					OnDone(false, StatusCode, false, false, TEXT(""), TEXT(""), TEXT(""),
						FFastGameHttp::ExtractApiMessage(StatusCode, Resp, Err));
				}
				return;
			}
			const TSharedPtr<FJsonObject> Obj = FastGameJsonUtil::ParseObject(Resp);
			if (!Obj.IsValid())
			{
				if (OnDone) OnDone(false, StatusCode, false, false, TEXT(""), TEXT(""), TEXT(""), TEXT("Enter response invalid"));
				return;
			}
			bool bExists = false;
			bool bPasswordRequired = false;
			Obj->TryGetBoolField(TEXT("exists"), bExists);
			Obj->TryGetBoolField(TEXT("password_required"), bPasswordRequired);
			FString ChannelStr;
			Obj->TryGetStringField(TEXT("channel"), ChannelStr);
			FString Email;
			FString Phone;
			Obj->TryGetStringField(TEXT("email"), Email);
			Obj->TryGetStringField(TEXT("phone"), Phone);
			const bool bIsEmail = ChannelStr.Equals(TEXT("email"), ESearchCase::IgnoreCase);
			const FString Stored = bIsEmail ? Email : Phone;
			if (!Stored.IsEmpty())
			{
				StoreEnteredIdentity(Stored, bIsEmail);
			}
			if (OnDone) OnDone(true, StatusCode, bExists, bPasswordRequired, ChannelStr, Email, Phone, TEXT(""));
		});
}

void FFastGameAuth::LoginWithEmail(const FString& Email, const FString& Password,
	TFunction<void(bool, int32, FString, FString)> OnDone)
{
	const FString Trimmed = Email.TrimStartAndEnd();
	if (!FastGameIdentity::LooksLikeEmail(Trimmed))
	{
		if (OnDone) OnDone(false, 0, TEXT(""), TEXT("Valid email is required"));
		return;
	}
	LoginWithUsername(Trimmed.ToLower(), Password, OnDone);
}

void FFastGameAuth::LoginWithPhone(const FString& Phone, const FString& Password,
	TFunction<void(bool, int32, FString, FString)> OnDone)
{
	FString Normalized;
	if (!FastGameIdentity::TryNormalizePhone(Phone, Normalized))
	{
		if (OnDone) OnDone(false, 0, TEXT(""), TEXT("Valid phone number is required"));
		return;
	}
	LoginWithUsername(Normalized, Password, OnDone);
}

void FFastGameAuth::LoginWithUsername(const FString& Username, const FString& Password,
	TFunction<void(bool, int32, FString, FString)> OnDone)
{
	if (Username.IsEmpty() || Password.IsEmpty())
	{
		if (OnDone) OnDone(false, 0, TEXT(""), TEXT("Email/phone and password are required"));
		return;
	}
	// Backend: POST /base/login/access-token — find_user_by_identity(username)
	TMap<FString, FString> Fields;
	Fields.Add(TEXT("username"), Username);
	Fields.Add(TEXT("password"), Password);
	TSharedRef<FFastGameHttp> HttpRef = Http;
	const FString TokenSlot = Config.AccessTokenSaveSlot;
	Http->PostForm(TEXT("/base/login/access-token"), Fields,
		[HttpRef, TokenSlot, OnDone](bool bOk, int32 StatusCode, FString Body, FString Err)
		{
			if (!bOk)
			{
				if (OnDone) OnDone(false, StatusCode, TEXT(""), FFastGameHttp::ExtractApiMessage(StatusCode, Body, Err));
				return;
			}
			const TSharedPtr<FJsonObject> Obj = FastGameJsonUtil::ParseObject(Body);
			FString Token;
			if (!Obj.IsValid() || !Obj->TryGetStringField(TEXT("access_token"), Token) || Token.IsEmpty())
			{
				if (OnDone) OnDone(false, StatusCode, TEXT(""), TEXT("Login response missing access_token"));
				return;
			}
			HttpRef->SetAccessToken(Token);
			FastGameJsonUtil::SaveAccessTokenFile(TokenSlot, Token);
			UE_LOG(LogTemp, Log, TEXT("FastGame: access token saved (slot=%s)"), *TokenSlot);
			if (OnDone) OnDone(true, StatusCode, Token, TEXT(""));
		});
}

void FFastGameAuth::Signup(const FString& Email, const FString& Phone, const FString& Password,
	const FString& PasswordConfirm, const FString& FullName,
	TFunction<void(bool, int32, FString, FString, FString, FString, FString)> OnDone)
{
	FString TrimEmail = Email.TrimStartAndEnd();
	FString TrimPhone = Phone.TrimStartAndEnd();
	if (TrimEmail.IsEmpty() && TrimPhone.IsEmpty())
	{
		FString EnterErr;
		if (!FillEmailPhoneFromEntered(TrimEmail, TrimPhone, EnterErr))
		{
			if (OnDone) OnDone(false, 0, TEXT(""), TEXT(""), TEXT(""), TEXT(""), EnterErr);
			return;
		}
	}
	if (!TrimEmail.IsEmpty())
	{
		if (!FastGameIdentity::LooksLikeEmail(TrimEmail))
		{
			if (OnDone) OnDone(false, 0, TEXT(""), TEXT(""), TEXT(""), TEXT(""), TEXT("Invalid email format"));
			return;
		}
		TrimEmail = TrimEmail.ToLower();
	}
	if (!TrimPhone.IsEmpty())
	{
		FString Normalized;
		if (!FastGameIdentity::TryNormalizePhone(TrimPhone, Normalized))
		{
			if (OnDone) OnDone(false, 0, TEXT(""), TEXT(""), TEXT(""), TEXT(""), TEXT("Invalid phone format"));
			return;
		}
		TrimPhone = Normalized;
	}
	FString PwErr;
	if (!FastGameIdentity::RequireMatchingPasswords(Password, PasswordConfirm, PwErr))
	{
		if (OnDone) OnDone(false, 0, TEXT(""), TEXT(""), TEXT(""), TEXT(""), PwErr);
		return;
	}

	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	if (!TrimEmail.IsEmpty())
	{
		Body->SetStringField(TEXT("email"), TrimEmail);
	}
	if (!TrimPhone.IsEmpty())
	{
		Body->SetStringField(TEXT("phone"), TrimPhone);
	}
	Body->SetStringField(TEXT("password"), Password);
	if (!FullName.IsEmpty())
	{
		Body->SetStringField(TEXT("full_name"), FullName);
	}
	const FString TrimGame = Config.GameCode.TrimStartAndEnd();
	if (!TrimGame.IsEmpty())
	{
		Body->SetStringField(TEXT("game_code"), TrimGame);
	}

	const FString LoginId = !TrimEmail.IsEmpty() ? TrimEmail : TrimPhone;
	TSharedRef<FFastGameHttp> HttpRef = Http;
	const FString TokenSlot = Config.AccessTokenSaveSlot;
	Http->PostJson(TEXT("/base/users/signup"), FastGameJsonUtil::Stringify(Body),
		[HttpRef, TokenSlot, TrimEmail, TrimPhone, LoginId, Password, OnDone](bool bOk, int32 StatusCode, FString Resp, FString Err)
		{
			if (!bOk)
			{
				if (OnDone) OnDone(false, StatusCode, TEXT(""), TEXT(""), TEXT(""), TEXT(""),
					FFastGameHttp::ExtractApiMessage(StatusCode, Resp, Err));
				return;
			}
			const TSharedPtr<FJsonObject> Obj = FastGameJsonUtil::ParseObject(Resp);
			FString UserId;
			FString OutEmail = TrimEmail;
			FString OutPhone = TrimPhone;
			if (Obj.IsValid())
			{
				Obj->TryGetStringField(TEXT("id"), UserId);
				Obj->TryGetStringField(TEXT("email"), OutEmail);
				Obj->TryGetStringField(TEXT("phone"), OutPhone);
			}

			TMap<FString, FString> Fields;
			Fields.Add(TEXT("username"), LoginId);
			Fields.Add(TEXT("password"), Password);
			HttpRef->PostForm(TEXT("/base/login/access-token"), Fields,
				[HttpRef, TokenSlot, UserId, OutEmail, OutPhone, StatusCode, OnDone](bool bLoginOk, int32 LoginCode, FString LoginBody, FString LoginErr)
				{
					if (!bLoginOk)
					{
						const FString Msg = FFastGameHttp::ExtractApiMessage(LoginCode, LoginBody, LoginErr);
						if (OnDone) OnDone(false, LoginCode > 0 ? LoginCode : StatusCode, UserId, OutEmail, OutPhone, TEXT(""),
							Msg.IsEmpty() ? TEXT("Signup succeeded but auto-login failed") : Msg);
						return;
					}
					const TSharedPtr<FJsonObject> LoginObj = FastGameJsonUtil::ParseObject(LoginBody);
					FString Token;
					if (!LoginObj.IsValid() || !LoginObj->TryGetStringField(TEXT("access_token"), Token) || Token.IsEmpty())
					{
						if (OnDone) OnDone(false, LoginCode, UserId, OutEmail, OutPhone, TEXT(""),
							TEXT("Signup succeeded but auto-login missing access_token"));
						return;
					}
					HttpRef->SetAccessToken(Token);
					FastGameJsonUtil::SaveAccessTokenFile(TokenSlot, Token);
					UE_LOG(LogTemp, Log, TEXT("FastGame: access token saved after signup (slot=%s)"), *TokenSlot);
					if (OnDone) OnDone(true, LoginCode > 0 ? LoginCode : StatusCode, UserId, OutEmail, OutPhone, Token, TEXT(""));
				});
		});
}

void FFastGameAuth::CompleteAccount(const FString& Email, const FString& Phone, const FString& Password,
	const FString& PasswordConfirm, const FString& FullName,
	TFunction<void(bool, int32, FString, FString, FString, FString, FString)> OnDone)
{
	FString TrimEmail = Email.TrimStartAndEnd();
	FString TrimPhone = Phone.TrimStartAndEnd();
	if (TrimEmail.IsEmpty() && TrimPhone.IsEmpty())
	{
		FString EnterErr;
		if (!FillEmailPhoneFromEntered(TrimEmail, TrimPhone, EnterErr))
		{
			if (OnDone) OnDone(false, 0, TEXT(""), TEXT(""), TEXT(""), TEXT(""), EnterErr);
			return;
		}
	}
	if (!TrimEmail.IsEmpty())
	{
		if (!FastGameIdentity::LooksLikeEmail(TrimEmail))
		{
			if (OnDone) OnDone(false, 0, TEXT(""), TEXT(""), TEXT(""), TEXT(""), TEXT("Invalid email format"));
			return;
		}
		TrimEmail = TrimEmail.ToLower();
	}
	if (!TrimPhone.IsEmpty())
	{
		FString Normalized;
		if (!FastGameIdentity::TryNormalizePhone(TrimPhone, Normalized))
		{
			if (OnDone) OnDone(false, 0, TEXT(""), TEXT(""), TEXT(""), TEXT(""), TEXT("Invalid phone format"));
			return;
		}
		TrimPhone = Normalized;
	}
	FString PwErr;
	if (!FastGameIdentity::RequireMatchingPasswords(Password, PasswordConfirm, PwErr))
	{
		if (OnDone) OnDone(false, 0, TEXT(""), TEXT(""), TEXT(""), TEXT(""), PwErr);
		return;
	}

	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	if (!TrimEmail.IsEmpty())
	{
		Body->SetStringField(TEXT("email"), TrimEmail);
	}
	if (!TrimPhone.IsEmpty())
	{
		Body->SetStringField(TEXT("phone"), TrimPhone);
	}
	Body->SetStringField(TEXT("password"), Password);
	if (!FullName.IsEmpty())
	{
		Body->SetStringField(TEXT("full_name"), FullName);
	}

	const FString LoginId = !TrimEmail.IsEmpty() ? TrimEmail : TrimPhone;
	TSharedRef<FFastGameHttp> HttpRef = Http;
	const FString TokenSlot = Config.AccessTokenSaveSlot;
	Http->PostJson(TEXT("/base/login/complete"), FastGameJsonUtil::Stringify(Body),
		[HttpRef, TokenSlot, TrimEmail, TrimPhone, LoginId, Password, OnDone](bool bOk, int32 StatusCode, FString Resp, FString Err)
		{
			if (!bOk)
			{
				if (OnDone) OnDone(false, StatusCode, TEXT(""), TEXT(""), TEXT(""), TEXT(""),
					FFastGameHttp::ExtractApiMessage(StatusCode, Resp, Err));
				return;
			}
			const TSharedPtr<FJsonObject> Obj = FastGameJsonUtil::ParseObject(Resp);
			FString UserId;
			FString OutEmail = TrimEmail;
			FString OutPhone = TrimPhone;
			if (Obj.IsValid())
			{
				Obj->TryGetStringField(TEXT("id"), UserId);
				Obj->TryGetStringField(TEXT("email"), OutEmail);
				Obj->TryGetStringField(TEXT("phone"), OutPhone);
			}

			TMap<FString, FString> Fields;
			Fields.Add(TEXT("username"), LoginId);
			Fields.Add(TEXT("password"), Password);
			HttpRef->PostForm(TEXT("/base/login/access-token"), Fields,
				[HttpRef, TokenSlot, UserId, OutEmail, OutPhone, StatusCode, OnDone](bool bLoginOk, int32 LoginCode, FString LoginBody, FString LoginErr)
				{
					if (!bLoginOk)
					{
						const FString Msg = FFastGameHttp::ExtractApiMessage(LoginCode, LoginBody, LoginErr);
						if (OnDone) OnDone(false, LoginCode > 0 ? LoginCode : StatusCode, UserId, OutEmail, OutPhone, TEXT(""),
							Msg.IsEmpty() ? TEXT("Complete Account succeeded but auto-login failed") : Msg);
						return;
					}
					const TSharedPtr<FJsonObject> LoginObj = FastGameJsonUtil::ParseObject(LoginBody);
					FString Token;
					if (!LoginObj.IsValid() || !LoginObj->TryGetStringField(TEXT("access_token"), Token) || Token.IsEmpty())
					{
						if (OnDone) OnDone(false, LoginCode, UserId, OutEmail, OutPhone, TEXT(""),
							TEXT("Complete Account succeeded but auto-login missing access_token"));
						return;
					}
					HttpRef->SetAccessToken(Token);
					FastGameJsonUtil::SaveAccessTokenFile(TokenSlot, Token);
					if (OnDone) OnDone(true, LoginCode > 0 ? LoginCode : StatusCode, UserId, OutEmail, OutPhone, Token, TEXT(""));
				});
		});
}

void FFastGameAuth::RequestPasswordRecovery(const FString& Identity,
	TFunction<void(bool, int32, FString)> OnDone)
{
	FString OutEmail, OutPhone, Err;
	if (!ResolveContactFields(Identity, TEXT(""), TEXT(""), OutEmail, OutPhone, Err))
	{
		if (OnDone) OnDone(false, 0, Err);
		return;
	}
	PostContactJson(TEXT("/base/recovery/request"), OutEmail, OutPhone, nullptr, OnDone);
}

void FFastGameAuth::VerifyPasswordRecovery(const FString& Identity, const FString& Code,
	TFunction<void(bool, int32, FString)> OnDone)
{
	if (Code.TrimStartAndEnd().IsEmpty())
	{
		if (OnDone) OnDone(false, 0, TEXT("Verification code is required"));
		return;
	}
	FString OutEmail, OutPhone, Err;
	if (!ResolveContactFields(Identity, TEXT(""), TEXT(""), OutEmail, OutPhone, Err))
	{
		if (OnDone) OnDone(false, 0, Err);
		return;
	}
	const FString CodeTrim = Code.TrimStartAndEnd();
	PostContactJson(TEXT("/base/recovery/verify"), OutEmail, OutPhone,
		[CodeTrim](TSharedPtr<FJsonObject> Body)
		{
			Body->SetStringField(TEXT("code"), CodeTrim);
		},
		OnDone);
}

void FFastGameAuth::ConfirmPasswordRecovery(const FString& Identity, const FString& Code,
	const FString& NewPassword, const FString& NewPasswordConfirm,
	TFunction<void(bool, int32, FString)> OnDone)
{
	FString PwErr;
	if (!FastGameIdentity::RequireMatchingPasswords(NewPassword, NewPasswordConfirm, PwErr))
	{
		if (OnDone) OnDone(false, 0, PwErr);
		return;
	}
	FString OutEmail, OutPhone, Err;
	if (!ResolveContactFields(Identity, TEXT(""), TEXT(""), OutEmail, OutPhone, Err))
	{
		if (OnDone) OnDone(false, 0, Err);
		return;
	}
	const FString CodeTrim = Code.TrimStartAndEnd();
	const FString LoginId = !OutEmail.IsEmpty() ? OutEmail : OutPhone;
	PostContactJson(TEXT("/base/recovery/confirm"), OutEmail, OutPhone,
		[CodeTrim, NewPassword](TSharedPtr<FJsonObject> Body)
		{
			if (!CodeTrim.IsEmpty())
			{
				Body->SetStringField(TEXT("code"), CodeTrim);
			}
			Body->SetStringField(TEXT("new_password"), NewPassword);
		},
		[this, LoginId, NewPassword, OnDone](bool bOk, int32 Status, FString Msg)
		{
			if (!bOk)
			{
				if (OnDone) OnDone(false, Status, Msg);
				return;
			}
			LoginWithUsername(LoginId, NewPassword,
				[OnDone, Status](bool bLoginOk, int32 LoginCode, FString /*Token*/, FString LoginErr)
				{
					if (!bLoginOk)
					{
						if (OnDone) OnDone(false, LoginCode > 0 ? LoginCode : Status,
							LoginErr.IsEmpty() ? TEXT("Password set but auto-login failed") : LoginErr);
						return;
					}
					if (OnDone) OnDone(true, LoginCode > 0 ? LoginCode : Status, TEXT(""));
				});
		});
}

void FFastGameAuth::RequestSignupVerification(const FString& Identity,
	TFunction<void(bool, int32, FString)> OnDone)
{
	FString OutEmail, OutPhone, Err;
	if (!ResolveContactFields(Identity, TEXT(""), TEXT(""), OutEmail, OutPhone, Err))
	{
		if (OnDone) OnDone(false, 0, Err);
		return;
	}
	PostContactJson(TEXT("/base/signup/request"), OutEmail, OutPhone, nullptr, OnDone);
}

void FFastGameAuth::VerifySignupVerification(const FString& Identity, const FString& Code,
	TFunction<void(bool, int32, FString)> OnDone)
{
	if (Code.TrimStartAndEnd().IsEmpty())
	{
		if (OnDone) OnDone(false, 0, TEXT("Verification code is required"));
		return;
	}
	FString OutEmail, OutPhone, Err;
	if (!ResolveContactFields(Identity, TEXT(""), TEXT(""), OutEmail, OutPhone, Err))
	{
		if (OnDone) OnDone(false, 0, Err);
		return;
	}
	const FString CodeTrim = Code.TrimStartAndEnd();
	PostContactJson(TEXT("/base/signup/verify"), OutEmail, OutPhone,
		[CodeTrim](TSharedPtr<FJsonObject> Body)
		{
			Body->SetStringField(TEXT("code"), CodeTrim);
		},
		OnDone);
}

void FFastGameAuth::GetMe(TFunction<void(bool, int32, FFastGameUser, FString)> OnDone)
{
	Http->Get(TEXT("/base/login/me"),
		[OnDone](bool bOk, int32 StatusCode, FString Body, FString Err)
		{
			FFastGameUser User;
			if (!bOk)
			{
				if (OnDone) OnDone(false, StatusCode, User,
					FFastGameHttp::ExtractApiMessage(StatusCode, Body, Err));
				return;
			}
			const TSharedPtr<FJsonObject> Obj = FastGameJsonUtil::ParseObject(Body);
			if (!Obj.IsValid())
			{
				if (OnDone) OnDone(false, StatusCode, User, TEXT("GetMe response invalid"));
				return;
			}
			Obj->TryGetStringField(TEXT("id"), User.Id);
			Obj->TryGetStringField(TEXT("email"), User.Email);
			Obj->TryGetStringField(TEXT("phone"), User.Phone);
			Obj->TryGetBoolField(TEXT("email_verified"), User.bEmailVerified);
			Obj->TryGetBoolField(TEXT("phone_verified"), User.bPhoneVerified);
			Obj->TryGetStringField(TEXT("full_name"), User.FullName);
			Obj->TryGetBoolField(TEXT("is_active"), User.bIsActive);
			Obj->TryGetBoolField(TEXT("is_superuser"), User.bIsSuperuser);
			if (OnDone) OnDone(true, StatusCode, User, TEXT(""));
		});
}

void FFastGameAuth::UpdateFullName(const FString& FullName,
	TFunction<void(bool, int32, FFastGameUser, FString)> OnDone)
{
	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("full_name"), FullName);
	Http->PatchJson(TEXT("/base/login/me"), FastGameJsonUtil::Stringify(Body),
		[OnDone](bool bOk, int32 StatusCode, FString Resp, FString Err)
		{
			FFastGameUser User;
			if (!bOk)
			{
				if (OnDone) OnDone(false, StatusCode, User,
					FFastGameHttp::ExtractApiMessage(StatusCode, Resp, Err));
				return;
			}
			const TSharedPtr<FJsonObject> Obj = FastGameJsonUtil::ParseObject(Resp);
			if (!Obj.IsValid())
			{
				if (OnDone) OnDone(false, StatusCode, User, TEXT("Update Full Name response invalid"));
				return;
			}
			Obj->TryGetStringField(TEXT("id"), User.Id);
			Obj->TryGetStringField(TEXT("email"), User.Email);
			Obj->TryGetStringField(TEXT("phone"), User.Phone);
			Obj->TryGetBoolField(TEXT("email_verified"), User.bEmailVerified);
			Obj->TryGetBoolField(TEXT("phone_verified"), User.bPhoneVerified);
			Obj->TryGetStringField(TEXT("full_name"), User.FullName);
			Obj->TryGetBoolField(TEXT("is_active"), User.bIsActive);
			Obj->TryGetBoolField(TEXT("is_superuser"), User.bIsSuperuser);
			if (OnDone) OnDone(true, StatusCode, User, TEXT(""));
		});
}

void FFastGameAuth::LinkSteamWithTicket(const FString& Ticket, const FString& Identity,
	TFunction<void(bool, bool, FString, FString)> OnDone)
{
	FString GameCode, GameErr;
	if (!RequireGameCode(GameCode, GameErr))
	{
		if (OnDone) OnDone(false, false, TEXT(""), GameErr);
		return;
	}
	const TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("ticket"), Ticket);
	Body->SetStringField(TEXT("game_code"), GameCode);
	if (!Identity.IsEmpty())
	{
		Body->SetStringField(TEXT("identity"), Identity);
	}
	Http->PostJson(TEXT("/base/steam/link"), FastGameJsonUtil::Stringify(Body),
		[OnDone](bool bOk, int32, FString Resp, FString Err)
		{
			if (!bOk)
			{
				if (OnDone) OnDone(false, false, TEXT(""), Err);
				return;
			}
			const TSharedPtr<FJsonObject> Obj = FastGameJsonUtil::ParseObject(Resp);
			bool bLinked = false;
			FString SteamId;
			if (Obj.IsValid())
			{
				Obj->TryGetBoolField(TEXT("linked"), bLinked);
				Obj->TryGetStringField(TEXT("steamid"), SteamId);
			}
			if (OnDone) OnDone(true, bLinked, SteamId, TEXT(""));
		});
}

void FFastGameAuth::GetSteamStatus(TFunction<void(bool, bool, FString, FString)> OnDone)
{
	Http->Get(TEXT("/base/steam/status"),
		[OnDone](bool bOk, int32, FString Resp, FString Err)
		{
			if (!bOk)
			{
				if (OnDone) OnDone(false, false, TEXT(""), Err);
				return;
			}
			const TSharedPtr<FJsonObject> Obj = FastGameJsonUtil::ParseObject(Resp);
			bool bLinked = false;
			FString SteamId;
			if (Obj.IsValid())
			{
				Obj->TryGetBoolField(TEXT("linked"), bLinked);
				Obj->TryGetStringField(TEXT("steamid"), SteamId);
			}
			if (OnDone) OnDone(true, bLinked, SteamId, TEXT(""));
		});
}

void FFastGameAuth::UnlinkSteam(TFunction<void(bool, FString)> OnDone)
{
	Http->Request(TEXT("DELETE"), TEXT("/base/steam/link"), TEXT(""), TEXT("application/json"),
		[OnDone](bool bOk, int32, FString, FString Err)
		{
			if (OnDone) OnDone(bOk, bOk ? TEXT("") : Err);
		});
}

void FFastGameCatalog::ListGames(bool bAvailableOnly, TFunction<void(bool, TArray<FFastGameCatalogEntry>, FString)> OnDone,
	const FString& Lang, bool bExpandI18n)
{
	FString Path = FString::Printf(TEXT("/apps/games/catalog/?available_only=%s"), bAvailableOnly ? TEXT("true") : TEXT("false"));
	Path = FFastGameHttp::AppendI18nQuery(Path, Lang, bExpandI18n);
	Http->Get(Path, [OnDone](bool bOk, int32, FString Body, FString Err)
	{
		TArray<FFastGameCatalogEntry> List;
		if (!bOk)
		{
			if (OnDone) OnDone(false, List, Err);
			return;
		}
		for (const auto& V : FastGameJsonUtil::ParseArray(Body))
		{
			List.Add(FastGameJsonUtil::ParseCatalog(V->AsObject()));
		}
		if (OnDone) OnDone(true, List, TEXT(""));
	});
}

void FFastGameCatalog::GetGame(const FString& GameId, TFunction<void(bool, FFastGameCatalogDetail, FString)> OnDone,
	const FString& Lang, bool bExpandI18n)
{
	const FString Path = FFastGameHttp::AppendI18nQuery(
		TEXT("/apps/games/catalog/") + FastGameJsonUtil::Escape(GameId), Lang, bExpandI18n);
	Http->Get(Path,
		[OnDone](bool bOk, int32, FString Body, FString Err)
		{
			FFastGameCatalogDetail Detail;
			if (!bOk)
			{
				if (OnDone) OnDone(false, Detail, Err);
				return;
			}
			Detail = FastGameJsonUtil::ParseCatalogDetail(FastGameJsonUtil::ParseObject(Body));
			if (OnDone) OnDone(true, Detail, TEXT(""));
		});
}

void FFastGameCatalog::GetAuthRequirements(const FString& GameId,
	TFunction<void(bool, bool, bool, FString)> OnDone)
{
	const FString Path = TEXT("/apps/games/catalog/") + FastGameJsonUtil::Escape(GameId)
		+ TEXT("/auth-requirements");
	Http->Get(Path,
		[OnDone](bool bOk, int32, FString Body, FString Err)
		{
			if (!bOk)
			{
				if (OnDone) OnDone(false, false, false, Err);
				return;
			}
			const TSharedPtr<FJsonObject> Obj = FastGameJsonUtil::ParseObject(Body);
			bool bPhone = false;
			bool bEmail = false;
			if (Obj.IsValid())
			{
				Obj->TryGetBoolField(TEXT("verify_phone"), bPhone);
				Obj->TryGetBoolField(TEXT("verify_email"), bEmail);
			}
			if (OnDone) OnDone(true, bPhone, bEmail, TEXT(""));
		});
}

void FFastGameCatalog::GetGameServer(TFunction<void(bool, FString, FString)> OnDone)
{
	Http->Get(TEXT("/utils/game-server/"), [OnDone](bool bOk, int32, FString Body, FString Err)
	{
		if (!bOk)
		{
			if (OnDone) OnDone(false, TEXT(""), Err);
			return;
		}
		const TSharedPtr<FJsonObject> Obj = FastGameJsonUtil::ParseObject(Body);
		FString Url;
		if (Obj.IsValid()) Obj->TryGetStringField(TEXT("url"), Url);
		if (OnDone) OnDone(true, Url, TEXT(""));
	});
}

void FFastGameRealtime::MintSeat(const FString& GameCode, const FString& MapId, const FString& ModeId,
	TFunction<void(bool, FFastGameSeatMint, FString)> OnDone)
{
	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("game_code"), GameCode);
	Body->SetStringField(TEXT("map_id"), MapId);
	if (!ModeId.IsEmpty())
	{
		Body->SetStringField(TEXT("mode_id"), ModeId);
	}
	Http->PostJson(TEXT("/apps/games/realtime/seat"), FastGameJsonUtil::Stringify(Body),
		[OnDone](bool bOk, int32, FString RespBody, FString Err)
		{
			FFastGameSeatMint Seat;
			if (!bOk)
			{
				if (OnDone) OnDone(false, Seat, Err);
				return;
			}
			const TSharedPtr<FJsonObject> Obj = FastGameJsonUtil::ParseObject(RespBody);
			if (Obj.IsValid())
			{
				Obj->TryGetStringField(TEXT("seat_token"), Seat.SeatToken);
				Obj->TryGetStringField(TEXT("expires_at"), Seat.ExpiresAt);
				Obj->TryGetStringField(TEXT("game_server_url"), Seat.GameServerUrl);
				Obj->TryGetStringField(TEXT("room_name"), Seat.RoomName);
				Obj->TryGetStringField(TEXT("game_id"), Seat.GameId);
				Obj->TryGetStringField(TEXT("map_id"), Seat.MapId);
				Obj->TryGetStringField(TEXT("mode_id"), Seat.ModeId);
			}
			if (OnDone) OnDone(true, Seat, TEXT(""));
		});
}

void FFastGameProgress::Get(const FString& GameCode, const FString& MapId,
	TFunction<void(bool, TSharedPtr<FJsonObject>, FString)> OnDone)
{
	FString Path = TEXT("/apps/games/progress/") + FastGameJsonUtil::Escape(GameCode);
	if (!MapId.IsEmpty())
	{
		Path += TEXT("?map_id=") + FastGameJsonUtil::Escape(MapId);
	}
	Http->Get(Path,
		[OnDone](bool bOk, int32, FString Body, FString Err)
		{
			if (!bOk)
			{
				if (OnDone) OnDone(false, nullptr, Err);
				return;
			}
			if (OnDone) OnDone(true, FastGameJsonUtil::ParseObject(Body), TEXT(""));
		});
}

void FFastGameProgress::Save(const FString& GameCode, const FString& EventType, const FString& MapId,
	TSharedPtr<FJsonObject> Payload,
	TFunction<void(bool, TSharedPtr<FJsonObject>, FString)> OnDone)
{
	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("event_type"), EventType);
	Body->SetStringField(TEXT("map_id"), MapId);
	Body->SetObjectField(TEXT("payload"), Payload.IsValid() ? Payload.ToSharedRef() : MakeShared<FJsonObject>());
	Http->PostJson(
		TEXT("/apps/games/progress/") + FastGameJsonUtil::Escape(GameCode) + TEXT("/events"),
		FastGameJsonUtil::Stringify(Body),
		[OnDone](bool bOk, int32, FString RespBody, FString Err)
		{
			if (!bOk)
			{
				if (OnDone) OnDone(false, nullptr, Err);
				return;
			}
			if (OnDone) OnDone(true, FastGameJsonUtil::ParseObject(RespBody), TEXT(""));
		});
}

void FFastGameContent::GetBootstrap(const FString& GameCode,
	TFunction<void(bool, TSharedPtr<FJsonObject>, FString)> OnDone)
{
	Http->Get(
		TEXT("/apps/games/tip/") + FastGameJsonUtil::Escape(GameCode) + TEXT("/bootstrap"),
		[OnDone](bool bOk, int32, FString Body, FString Err)
		{
			if (!bOk)
			{
				if (OnDone) OnDone(false, nullptr, Err);
				return;
			}
			if (OnDone) OnDone(true, FastGameJsonUtil::ParseObject(Body), TEXT(""));
		});
}

void FFastGameContent::GetGameConfig(const FString& GameCode,
	TFunction<void(bool, TSharedPtr<FJsonObject>, FString)> OnDone)
{
	Http->Get(
		TEXT("/apps/games/tip/") + FastGameJsonUtil::Escape(GameCode) + TEXT("/game"),
		[OnDone](bool bOk, int32, FString Body, FString Err)
		{
			if (!bOk)
			{
				if (OnDone) OnDone(false, nullptr, Err);
				return;
			}
			if (OnDone) OnDone(true, FastGameJsonUtil::ParseObject(Body), TEXT(""));
		});
}

void FFastGameContent::GetPackTip(const FString& GameCode, const FString& PackId,
	TFunction<void(bool, TSharedPtr<FJsonObject>, FString)> OnDone)
{
	Http->Get(
		TEXT("/apps/games/asset-packs/") + FastGameJsonUtil::Escape(GameCode) + TEXT("/packs/")
			+ FastGameJsonUtil::Escape(PackId),
		[OnDone](bool bOk, int32, FString Body, FString Err)
		{
			if (!bOk)
			{
				if (OnDone) OnDone(false, nullptr, Err);
				return;
			}
			if (OnDone) OnDone(true, FastGameJsonUtil::ParseObject(Body), TEXT(""));
		});
}

void FFastGameContent::GetMapConfig(const FString& GameCode, const FString& MapId,
	TFunction<void(bool, TSharedPtr<FJsonObject>, FString)> OnDone)
{
	Http->Get(
		TEXT("/apps/games/tip/") + FastGameJsonUtil::Escape(GameCode) + TEXT("/maps/") +
			FastGameJsonUtil::Escape(MapId),
		[OnDone](bool bOk, int32, FString Body, FString Err)
		{
			if (!bOk)
			{
				if (OnDone) OnDone(false, nullptr, Err);
				return;
			}
			if (OnDone) OnDone(true, FastGameJsonUtil::ParseObject(Body), TEXT(""));
		});
}

void FFastGameContent::GetCharacter(const FString& GameCode, const FString& CharacterId,
	TFunction<void(bool, TSharedPtr<FJsonObject>, FString)> OnDone)
{
	Http->Get(
		TEXT("/apps/games/tip/") + FastGameJsonUtil::Escape(GameCode) + TEXT("/characters/") +
			FastGameJsonUtil::Escape(CharacterId),
		[OnDone](bool bOk, int32, FString Body, FString Err)
		{
			if (!bOk)
			{
				if (OnDone) OnDone(false, nullptr, Err);
				return;
			}
			if (OnDone) OnDone(true, FastGameJsonUtil::ParseObject(Body), TEXT(""));
		});
}

void FFastGameContent::GetDialogue(const FString& GameCode, const FString& DialogueId,
	TFunction<void(bool, TSharedPtr<FJsonObject>, FString)> OnDone)
{
	Http->Get(
		TEXT("/apps/games/tip/") + FastGameJsonUtil::Escape(GameCode) + TEXT("/dialogues/") +
			FastGameJsonUtil::Escape(DialogueId),
		[OnDone](bool bOk, int32, FString Body, FString Err)
		{
			if (!bOk)
			{
				if (OnDone) OnDone(false, nullptr, Err);
				return;
			}
			if (OnDone) OnDone(true, FastGameJsonUtil::ParseObject(Body), TEXT(""));
		});
}

void FFastGameContent::GetQuiz(const FString& GameCode, const FString& QuizId,
	TFunction<void(bool, TSharedPtr<FJsonObject>, FString)> OnDone)
{
	Http->Get(
		TEXT("/apps/games/tip/") + FastGameJsonUtil::Escape(GameCode) + TEXT("/quizzes/") +
			FastGameJsonUtil::Escape(QuizId),
		[OnDone](bool bOk, int32, FString Body, FString Err)
		{
			if (!bOk)
			{
				if (OnDone) OnDone(false, nullptr, Err);
				return;
			}
			if (OnDone) OnDone(true, FastGameJsonUtil::ParseObject(Body), TEXT(""));
		});
}

void FFastGameContent::GetStrings(const FString& GameCode, const FString& Context, const FString& Lang,
	TFunction<void(bool, TSharedPtr<FJsonObject>, FString)> OnDone)
{
	FString Path = TEXT("/apps/games/strings/") + FastGameJsonUtil::Escape(GameCode) +
		TEXT("?context=") + FastGameJsonUtil::Escape(Context.IsEmpty() ? TEXT("HOME") : Context) +
		TEXT("&lang=") + FastGameJsonUtil::Escape(Lang.IsEmpty() ? TEXT("en") : Lang);
	Http->Get(Path,
		[OnDone](bool bOk, int32, FString Body, FString Err)
		{
			if (!bOk)
			{
				if (OnDone) OnDone(false, nullptr, Err);
				return;
			}
			if (OnDone) OnDone(true, FastGameJsonUtil::ParseObject(Body), TEXT(""));
		});
}

void FFastGameContent::ListCharacters(const FString& GameId, TFunction<void(bool, TArray<FFastGameCharacter>, FString)> OnDone,
	const FString& Lang, bool bExpandI18n)
{
	ListCharacters(GameId, TEXT(""), OnDone, Lang, bExpandI18n);
}

void FFastGameContent::ListCharacters(const FString& GameId, const FString& Role,
	TFunction<void(bool, TArray<FFastGameCharacter>, FString)> OnDone,
	const FString& Lang, bool bExpandI18n)
{
	FString Path = TEXT("/apps/games/content/") + FastGameJsonUtil::Escape(GameId) + TEXT("/characters");
	if (!Role.IsEmpty())
	{
		Path += TEXT("?role=") + FastGameJsonUtil::Escape(Role);
	}
	Path = FFastGameHttp::AppendI18nQuery(Path, Lang, bExpandI18n);
	Http->Get(Path,
		[OnDone](bool bOk, int32, FString Body, FString Err)
		{
			TArray<FFastGameCharacter> List;
			if (!bOk)
			{
				if (OnDone) OnDone(false, List, Err);
				return;
			}
			for (const auto& V : FastGameJsonUtil::ParseArray(Body))
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				FFastGameCharacter C;
				if (!O.IsValid()) continue;
				O->TryGetStringField(TEXT("id"), C.Id);
				O->TryGetStringField(TEXT("character_id"), C.CharacterId);
				O->TryGetStringField(TEXT("label"), C.Label);
				if (!O->TryGetStringField(TEXT("role"), C.Role)) C.Role = TEXT("player");
				O->TryGetStringField(TEXT("body_kind"), C.BodyKind);
				double Sort = 0;
				O->TryGetNumberField(TEXT("sort_order"), Sort);
				C.SortOrder = static_cast<int32>(Sort);
				const TSharedPtr<FJsonObject>* Stats = nullptr;
				if (O->TryGetObjectField(TEXT("stats"), Stats) && Stats) C.Stats = *Stats;
				const TSharedPtr<FJsonObject>* Translations = nullptr;
				if (O->TryGetObjectField(TEXT("translations"), Translations) && Translations)
					C.Translations = *Translations;
				List.Add(C);
			}
			if (OnDone) OnDone(true, List, TEXT(""));
		});
}

void FFastGameContent::ClaimEvent(const FString& GameId, const FString& EventId,
	TFunction<void(bool, TSharedPtr<FJsonObject>, FString)> OnDone)
{
	Http->PostJson(
		TEXT("/apps/games/content/") + FastGameJsonUtil::Escape(GameId) + TEXT("/events/") +
			FastGameJsonUtil::Escape(EventId) + TEXT("/claim"),
		TEXT("{}"),
		[OnDone](bool bOk, int32, FString Body, FString Err)
		{
			if (!bOk)
			{
				if (OnDone) OnDone(false, nullptr, Err);
				return;
			}
			if (OnDone) OnDone(true, FastGameJsonUtil::ParseObject(Body), TEXT(""));
		});
}

void FFastGameContent::GetMapRuntime(const FString& GameId, const FString& MapId,
	TFunction<void(bool, TSharedPtr<FJsonObject>, FString)> OnDone,
	const FString& Lang, bool bExpandI18n)
{
	const FString Path = FFastGameHttp::AppendI18nQuery(
		TEXT("/apps/games/content/") + FastGameJsonUtil::Escape(GameId) + TEXT("/maps/") + FastGameJsonUtil::Escape(MapId) + TEXT("/runtime"),
		Lang,
		bExpandI18n);
	Http->Get(
		Path,
		[OnDone](bool bOk, int32, FString Body, FString Err)
		{
			if (!bOk)
			{
				if (OnDone) OnDone(false, nullptr, Err);
				return;
			}
			if (OnDone) OnDone(true, FastGameJsonUtil::ParseObject(Body), TEXT(""));
		});
}

void FFastGameContent::ResolveSpawn(const FString& GameId, const FString& MapId, const FString& ModeId,
	TFunction<void(bool, TSharedPtr<FJsonObject>, FString)> OnDone,
	const FString& Lang, bool bExpandI18n)
{
	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("map_id"), MapId);
	if (!ModeId.IsEmpty()) Body->SetStringField(TEXT("mode_id"), ModeId);
	const FString Path = FFastGameHttp::AppendI18nQuery(
		TEXT("/apps/games/content/") + FastGameJsonUtil::Escape(GameId) + TEXT("/players/me/spawn"),
		Lang,
		bExpandI18n);
	Http->PostJson(
		Path,
		FastGameJsonUtil::Stringify(Body),
		[OnDone](bool bOk, int32, FString Resp, FString Err)
		{
			if (!bOk)
			{
				if (OnDone) OnDone(false, nullptr, Err);
				return;
			}
			if (OnDone) OnDone(true, FastGameJsonUtil::ParseObject(Resp), TEXT(""));
		});
}

void FFastGameContent::GetLoadout(const FString& GameId, TFunction<void(bool, FFastGameLoadout, FString)> OnDone)
{
	Http->Get(TEXT("/apps/games/content/") + FastGameJsonUtil::Escape(GameId) + TEXT("/players/me/loadout"),
		[OnDone](bool bOk, int32, FString Body, FString Err)
		{
			FFastGameLoadout L;
			if (!bOk)
			{
				if (OnDone) OnDone(false, L, Err);
				return;
			}
			if (OnDone) OnDone(true, FastGameJsonUtil::ParseLoadout(FastGameJsonUtil::ParseObject(Body)), TEXT(""));
		});
}

void FFastGameContent::SetLoadout(const FString& GameId, const FString& CharacterId,
	const TMap<FString, FString>& Cosmetics, const TMap<FString, FString>& ModularParts,
	TFunction<void(bool, FFastGameLoadout, FString)> OnDone)
{
	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	if (!CharacterId.IsEmpty()) Body->SetStringField(TEXT("character_id"), CharacterId);
	TSharedPtr<FJsonObject> Cos = MakeShared<FJsonObject>();
	for (const auto& Kv : Cosmetics) Cos->SetStringField(Kv.Key, Kv.Value);
	Body->SetObjectField(TEXT("equipped_cosmetics"), Cos);
	TSharedPtr<FJsonObject> Parts = MakeShared<FJsonObject>();
	for (const auto& Kv : ModularParts) Parts->SetStringField(Kv.Key, Kv.Value);
	Body->SetObjectField(TEXT("modular_parts"), Parts);
	Http->PutJson(
		TEXT("/apps/games/content/") + FastGameJsonUtil::Escape(GameId) + TEXT("/players/me/loadout"),
		FastGameJsonUtil::Stringify(Body),
		[OnDone](bool bOk, int32, FString Resp, FString Err)
		{
			FFastGameLoadout L;
			if (!bOk)
			{
				if (OnDone) OnDone(false, L, Err);
				return;
			}
			if (OnDone) OnDone(true, FastGameJsonUtil::ParseLoadout(FastGameJsonUtil::ParseObject(Resp)), TEXT(""));
		});
}

void FFastGameContent::ClaimPickup(const FString& GameId, const FString& MapId, const FString& PickupId,
	const FString& PlacementId, TFunction<void(bool, TSharedPtr<FJsonObject>, FString)> OnDone)
{
	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("map_id"), MapId);
	Body->SetStringField(TEXT("pickup_id"), PickupId);
	if (!PlacementId.IsEmpty()) Body->SetStringField(TEXT("placement_id"), PlacementId);
	Http->PostJson(
		TEXT("/apps/games/content/") + FastGameJsonUtil::Escape(GameId) + TEXT("/players/me/pickup-claim"),
		FastGameJsonUtil::Stringify(Body),
		[OnDone](bool bOk, int32, FString Resp, FString Err)
		{
			if (!bOk)
			{
				if (OnDone) OnDone(false, nullptr, Err);
				return;
			}
			if (OnDone) OnDone(true, FastGameJsonUtil::ParseObject(Resp), TEXT(""));
		});
}

void FFastGameContent::PrepareSession(const FString& GameId, const FString& ModeId, const FString& MapId,
	TFunction<void(bool, FFastGamePreparedSession, FString)> OnDone,
	const FString& Lang, bool bExpandI18n)
{
	TSharedRef<FFastGameContent> Self = AsShared();
	Catalog->GetGame(GameId, [Self, GameId, ModeId, MapId, Lang, bExpandI18n, OnDone](bool bOk, FFastGameCatalogDetail Detail, FString Err)
	{
		if (!bOk)
		{
			if (OnDone) OnDone(false, {}, Err);
			return;
		}
		Self->GetMapRuntime(GameId, MapId, [Self, GameId, ModeId, MapId, Detail, Lang, bExpandI18n, OnDone](bool bOk2, TSharedPtr<FJsonObject> Runtime, FString Err2)
		{
			if (!bOk2)
			{
				if (OnDone) OnDone(false, {}, Err2);
				return;
			}
			Self->ResolveSpawn(GameId, MapId, ModeId, [Detail, Runtime, GameId, ModeId, MapId, OnDone](bool bOk3, TSharedPtr<FJsonObject> Spawn, FString Err3)
			{
				FFastGamePreparedSession Session;
				if (!bOk3)
				{
					if (OnDone) OnDone(false, Session, Err3);
					return;
				}
				Session.Game = Detail;
				Session.MapRuntime = Runtime;
				Session.Spawn = Spawn;
				Session.GameId = GameId;
				Session.ModeId = ModeId;
				Session.MapId = MapId;
				Session.ColyseusRoom = Detail.ColyseusRoom;
				if (OnDone) OnDone(true, Session, TEXT(""));
			}, Lang, bExpandI18n);
		}, Lang, bExpandI18n);
	}, Lang, bExpandI18n);
}

bool FFastGameShop::HasPendingPayment() const
{
	return FPaths::FileExists(FastGameJsonUtil::PendingPath(Config.PendingPaymentSaveSlot));
}

void FFastGameShop::GetCatalog(const FString& GameIdFilter, TFunction<void(bool, TArray<FFastGameShopLine>, FString)> OnDone,
	const FString& Lang, bool bExpandI18n)
{
	FString ResolvedFilter;
	FString FilterErr;
	if (!ResolveGameCode(GameIdFilter, ResolvedFilter, FilterErr))
	{
		if (OnDone) OnDone(false, {}, FilterErr);
		return;
	}
	FString Path = TEXT("/apps/games/shop/catalog");
	Path += FString::Printf(TEXT("?game_code=%s"), *FGenericPlatformHttp::UrlEncode(ResolvedFilter));
	Path = FFastGameHttp::AppendI18nQuery(Path, Lang, bExpandI18n);
	Http->Get(Path, [ResolvedFilter, OnDone](bool bOk, int32, FString Body, FString Err)
	{
		TArray<FFastGameShopLine> List;
		if (!bOk)
		{
			if (OnDone) OnDone(false, List, Err);
			return;
		}
		for (const auto& V : FastGameJsonUtil::ParseArray(Body))
		{
			FFastGameShopLine Line = FastGameJsonUtil::ParseShopLine(V->AsObject());
			if (!Line.GameCode.Equals(ResolvedFilter, ESearchCase::IgnoreCase))
			{
				continue;
			}
			List.Add(Line);
		}
		if (OnDone) OnDone(true, List, TEXT(""));
	});
}

bool FFastGameShop::ResolveGameCode(const FString& GameCode, FString& OutGameCode, FString& OutError) const
{
	OutGameCode = GameCode;
	OutGameCode.TrimStartAndEndInline();
	if (OutGameCode.IsEmpty())
	{
		OutGameCode = Config.GameCode;
		OutGameCode.TrimStartAndEndInline();
	}
	if (OutGameCode.IsEmpty())
	{
		OutError = TEXT("FastGame: GameCode not set — call Initialize Game");
		return false;
	}
	return true;
}

void FFastGameShop::BindStoreLock()
{
	FString GameCode;
	FString GameErr;
	if (!ResolveGameCode(TEXT(""), GameCode, GameErr))
	{
		return;
	}
	FString Provider;
	FString ProvErr;
	if (!ResolveProvider(TEXT(""), Provider, ProvErr))
	{
		return;
	}
	if (!FFastGameNativeStore::IsAndroidStoreProvider(Provider))
	{
		return;
	}
	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("game_code"), GameCode);
	Body->SetStringField(TEXT("provider"), Provider);
	Http->PostJson(TEXT("/apps/games/shop/store-lock"), FastGameJsonUtil::Stringify(Body),
		[](bool, int32, FString, FString)
		{
		});
	EnsureStoreVerifyKey([](bool, FString, FString)
	{
	});
}

void FFastGameShop::EnsureStoreVerifyKey(TFunction<void(bool, FString, FString)> OnDone)
{
	auto Done = [OnDone](bool bOk, const FString& Pem, const FString& Err)
	{
		if (OnDone) OnDone(bOk, Pem, Err);
	};
	if (!Config.StorePublicKey.TrimStartAndEnd().IsEmpty())
	{
		Done(true, Config.StorePublicKey, TEXT(""));
		return;
	}
	FString Provider;
	FString ProvErr;
	if (!ResolveProvider(TEXT(""), Provider, ProvErr) || !FastGameNeedsStoreRsa(Provider))
	{
		Done(true, TEXT(""), TEXT(""));
		return;
	}
	FString GameCode;
	FString GameErr;
	if (!ResolveGameCode(TEXT(""), GameCode, GameErr))
	{
		Done(false, TEXT(""), GameErr);
		return;
	}
	const FString Path = FString::Printf(
		TEXT("/apps/games/catalog/%s/store-verify-key?provider=%s"),
		*FGenericPlatformHttp::UrlEncode(GameCode),
		*FGenericPlatformHttp::UrlEncode(Provider));
	Http->Get(Path, [this, Done, GameCode, Provider](bool bOk, int32, FString Body, FString Err)
	{
		if (!bOk)
		{
			Done(false, TEXT(""), Err);
			return;
		}
		const TSharedPtr<FJsonObject> Obj = FastGameJsonUtil::ParseObject(Body);
		bool bConfigured = false;
		FString Wrapped;
		if (Obj.IsValid())
		{
			Obj->TryGetBoolField(TEXT("configured"), bConfigured);
			Obj->TryGetStringField(TEXT("rsa_verify_key"), Wrapped);
		}
		if (!bConfigured || Wrapped.TrimStartAndEnd().IsEmpty())
		{
			Done(true, TEXT(""), TEXT(""));
			return;
		}
		FString Pem;
		if (!FastGameUnwrapStoreVerifyKey(Wrapped, GameCode, Provider, Pem))
		{
			Done(false, TEXT(""), TEXT("FastGame: store RSA decode failed"));
			return;
		}
		Config.StorePublicKey = Pem;
		Done(true, Pem, TEXT(""));
	});
}

bool FFastGameShop::ResolveProvider(const FString& Provider, FString& OutProvider, FString& OutError) const
{
	OutProvider = FastGameNormalizeProviderId(Provider);
	if (OutProvider.IsEmpty())
	{
		OutProvider = FastGameNormalizeProviderId(Config.StorePlatform);
	}
	if (OutProvider.IsEmpty())
	{
		OutError = TEXT("FastGame: StorePlatform not set — call Initialize Game");
		return false;
	}
	return true;
}

void FFastGameShop::GetSkuAccess(const FString& GameCode, const FString& SkuKind, const FString& SkuId,
	TFunction<void(bool, bool, bool, const TArray<FString>&, FString)> OnDone)
{
	FString Resolved;
	FString Err;
	if (!ResolveGameCode(GameCode, Resolved, Err))
	{
		if (OnDone) OnDone(false, false, false, {}, Err);
		return;
	}
	BindStoreLock();
	FString Provider;
	FString ProvErr;
	ResolveProvider(TEXT(""), Provider, ProvErr);
	FString Path = FString::Printf(
		TEXT("/apps/games/shop/access?game_code=%s&sku_kind=%s&sku_id=%s"),
		*FGenericPlatformHttp::UrlEncode(Resolved),
		*FGenericPlatformHttp::UrlEncode(SkuKind),
		*FGenericPlatformHttp::UrlEncode(SkuId));
	if (!Provider.IsEmpty())
	{
		Path += FString::Printf(TEXT("&provider=%s"), *FGenericPlatformHttp::UrlEncode(Provider));
	}
	Http->Get(Path, [OnDone](bool bOk, int32, FString Body, FString Err)
	{
		bool Locked = false;
		bool Owned = false;
		TArray<FString> StoreProductIds;
		if (bOk)
		{
			TSharedPtr<FJsonObject> Obj = FastGameJsonUtil::ParseObject(Body);
			if (Obj.IsValid())
			{
				Obj->TryGetBoolField(TEXT("locked"), Locked);
				Obj->TryGetBoolField(TEXT("owned"), Owned);
				const TArray<TSharedPtr<FJsonValue>>* IdArray = nullptr;
				if (Obj->TryGetArrayField(TEXT("store_product_ids"), IdArray) && IdArray)
				{
					for (const TSharedPtr<FJsonValue>& Val : *IdArray)
					{
						FString Id;
						if (Val.IsValid() && Val->TryGetString(Id) && !Id.TrimStartAndEnd().IsEmpty())
						{
							StoreProductIds.AddUnique(Id.TrimStartAndEnd());
						}
					}
				}
				if (StoreProductIds.Num() == 0)
				{
					FString Single;
					if (Obj->TryGetStringField(TEXT("store_product_id"), Single) && !Single.TrimStartAndEnd().IsEmpty())
					{
						StoreProductIds.Add(Single.TrimStartAndEnd());
					}
				}
			}
		}
		if (OnDone) OnDone(bOk, Locked, Owned, StoreProductIds, Err);
	});
}

void FFastGameShop::ClaimFree(const FString& GameCode, const FString& SkuKind, const FString& SkuId,
	TFunction<void(bool, FString)> OnDone)
{
	FString Resolved;
	FString Err;
	if (!ResolveGameCode(GameCode, Resolved, Err))
	{
		if (OnDone) OnDone(false, Err);
		return;
	}
	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("game_code"), Resolved);
	Body->SetStringField(TEXT("sku_kind"), SkuKind);
	Body->SetStringField(TEXT("sku_id"), SkuId);
	Http->PostJson(TEXT("/apps/games/shop/claim-free"), FastGameJsonUtil::Stringify(Body),
		[OnDone](bool bOk, int32, FString, FString Err)
		{
			if (OnDone) OnDone(bOk, Err);
		});
}

void FFastGameShop::RedeemCode(const FString& GameCode, const FString& Code,
	TFunction<void(bool, FString)> OnDone)
{
	FString Resolved;
	FString Err;
	if (!ResolveGameCode(GameCode, Resolved, Err))
	{
		if (OnDone) OnDone(false, Err);
		return;
	}
	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("game_code"), Resolved);
	Body->SetStringField(TEXT("code"), Code);
	Http->PostJson(TEXT("/apps/games/shop/redeem-code"), FastGameJsonUtil::Stringify(Body),
		[OnDone](bool bOk, int32, FString, FString Err)
		{
			if (OnDone) OnDone(bOk, Err);
		});
}

void FFastGameShop::UnlockSku(const FString& GameCode, const FString& SkuKind, const FString& SkuId,
	const FString& CallbackUrl, const FString& DiscountCode,
	TFunction<void(bool, FFastGameShopUnlock, FString)> OnDone)
{
	FFastGameShopUnlock Empty;
	FString ResolvedGame;
	FString GameErr;
	if (!ResolveGameCode(GameCode, ResolvedGame, GameErr))
	{
		if (OnDone) OnDone(false, Empty, GameErr);
		return;
	}
	BindStoreLock();
	FString ResolvedProvider;
	FString ProvErr;
	if (!ResolveProvider(TEXT(""), ResolvedProvider, ProvErr))
	{
		if (OnDone) OnDone(false, Empty, ProvErr);
		return;
	}

	TSharedPtr<FJsonObject> CartLine = MakeShared<FJsonObject>();
	CartLine->SetStringField(TEXT("game_code"), ResolvedGame);
	CartLine->SetStringField(TEXT("sku_kind"), SkuKind);
	CartLine->SetStringField(TEXT("sku_id"), SkuId);
	TArray<TSharedPtr<FJsonValue>> Cart;
	Cart.Add(MakeShared<FJsonValueObject>(CartLine));

	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("game_code"), ResolvedGame);
	Body->SetStringField(TEXT("sku_kind"), SkuKind);
	Body->SetStringField(TEXT("sku_id"), SkuId);
	Body->SetStringField(TEXT("provider"), ResolvedProvider);
	if (!CallbackUrl.IsEmpty())
	{
		Body->SetStringField(TEXT("callback_url"), CallbackUrl);
	}
	if (!DiscountCode.IsEmpty())
	{
		Body->SetStringField(TEXT("discount_code"), DiscountCode);
	}

	const FString PendingSlot = Config.PendingPaymentSaveSlot;
	Http->PostJson(TEXT("/apps/games/shop/unlock/begin"), FastGameJsonUtil::Stringify(Body),
		[Cart, ResolvedProvider, PendingSlot, OnDone](bool bOk, int32, FString Resp, FString Err)
		{
			FFastGameShopUnlock Result;
			Result.Provider = ResolvedProvider;
			if (!bOk)
			{
				if (OnDone) OnDone(false, Result, Err);
				return;
			}
			const TSharedPtr<FJsonObject> Obj = FastGameJsonUtil::ParseObject(Resp);
			if (Obj.IsValid())
			{
				Obj->TryGetBoolField(TEXT("owned"), Result.bOwned);
				Obj->TryGetBoolField(TEXT("pending"), Result.bPending);
				Obj->TryGetBoolField(TEXT("locked"), Result.bLocked);
				Obj->TryGetStringField(TEXT("mode"), Result.Mode);
				Obj->TryGetStringField(TEXT("provider"), Result.Provider);
				Obj->TryGetStringField(TEXT("authority"), Result.Authority);
				Obj->TryGetStringField(TEXT("payment_token"), Result.PaymentToken);
				Obj->TryGetStringField(TEXT("payment_url"), Result.PaymentUrl);
				Obj->TryGetStringField(TEXT("store_product_id"), Result.StoreProductId);
				Obj->TryGetStringField(TEXT("order_id"), Result.OrderId);
				Obj->TryGetStringField(TEXT("currency"), Result.Currency);
				double Amount = 0;
				Obj->TryGetNumberField(TEXT("amount"), Amount);
				Result.Amount = static_cast<int32>(Amount);
			}
			if (Result.bPending && !Result.Authority.IsEmpty() && !Result.PaymentToken.IsEmpty())
			{
				TSharedPtr<FJsonObject> Pending = MakeShared<FJsonObject>();
				Pending->SetStringField(TEXT("authority"), Result.Authority);
				Pending->SetStringField(TEXT("payment_token"), Result.PaymentToken);
				Pending->SetStringField(TEXT("provider"), Result.Provider.IsEmpty() ? ResolvedProvider : Result.Provider);
				Pending->SetStringField(TEXT("mode"), Result.Mode);
				Pending->SetArrayField(TEXT("lines"), Cart);
				const FString PendingPath = FastGameJsonUtil::PendingPath(PendingSlot);
				IFileManager::Get().MakeDirectory(*FPaths::GetPath(PendingPath), true);
				FFileHelper::SaveStringToFile(FastGameJsonUtil::Stringify(Pending), *PendingPath);
			}
			if (!Result.PaymentUrl.IsEmpty())
			{
				FPlatformProcess::LaunchURL(*Result.PaymentUrl, nullptr, nullptr);
			}
			if (OnDone) OnDone(true, Result, TEXT(""));
		});
}

void FFastGameShop::CompleteUnlock(const FString& PurchaseToken, TFunction<void(bool, bool, FString)> OnDone)
{
	FString Raw;
	const FString Path = FastGameJsonUtil::PendingPath(Config.PendingPaymentSaveSlot);
	if (!FFileHelper::LoadFileToString(Raw, *Path))
	{
		if (OnDone) OnDone(false, false, TEXT("No pending payment"));
		return;
	}
	const TSharedPtr<FJsonObject> Pending = FastGameJsonUtil::ParseObject(Raw);
	if (!Pending.IsValid())
	{
		if (OnDone) OnDone(false, false, TEXT("Invalid pending payment"));
		return;
	}
	FString Authority, Token;
	Pending->TryGetStringField(TEXT("authority"), Authority);
	Pending->TryGetStringField(TEXT("payment_token"), Token);
	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("authority"), Authority);
	Body->SetStringField(TEXT("payment_token"), Token);
	if (!PurchaseToken.TrimStartAndEnd().IsEmpty())
	{
		Body->SetStringField(TEXT("purchase_token"), PurchaseToken.TrimStartAndEnd());
	}
	Http->PostJson(TEXT("/apps/games/shop/unlock/complete"), FastGameJsonUtil::Stringify(Body),
		[Path, OnDone](bool bOk, int32 Code, FString Resp, FString Err)
		{
			if (!bOk)
			{
				int32 ParsedCode = Code;
				FString Msg;
				FFastGameHttp::ParseStatusFromError(bOk, Err, ParsedCode, Msg);
				if (OnDone) OnDone(false, false, Msg.IsEmpty() ? Err : Msg);
				return;
			}
			bool bSuccess = false;
			bool bOwned = false;
			const TSharedPtr<FJsonObject> Obj = FastGameJsonUtil::ParseObject(Resp);
			if (Obj.IsValid())
			{
				Obj->TryGetBoolField(TEXT("success"), bSuccess);
				Obj->TryGetBoolField(TEXT("owned"), bOwned);
			}
			if (bSuccess)
			{
				IFileManager::Get().Delete(*Path);
			}
			const bool bUnlocked = bOwned || bSuccess;
			if (bUnlocked)
			{
				if (OnDone) OnDone(true, true, TEXT(""));
				return;
			}
			const FString ClientMsg = FFastGameHttp::ExtractShopUnlockMessage(
				Resp, TEXT("Purchase validation failed"));
			if (OnDone) OnDone(true, false, ClientMsg);
		});
}

void FFastGameShop::RestoreUnlock(const FString& GameCode, const FString& SkuKind, const FString& SkuId,
	const FString& PurchaseToken, const FString& StoreProductId,
	TFunction<void(bool, bool, FString)> OnDone)
{
	FString ResolvedGame;
	FString GameErr;
	if (!ResolveGameCode(GameCode, ResolvedGame, GameErr))
	{
		if (OnDone) OnDone(false, false, GameErr);
		return;
	}
	FString ResolvedProvider;
	FString ProvErr;
	if (!ResolveProvider(TEXT(""), ResolvedProvider, ProvErr))
	{
		if (OnDone) OnDone(false, false, ProvErr);
		return;
	}
	const FString Token = PurchaseToken.TrimStartAndEnd();
	if (Token.IsEmpty())
	{
		if (OnDone) OnDone(false, false, TEXT("FastGame: purchase_token required to restore store ownership"));
		return;
	}
	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("game_code"), ResolvedGame);
	Body->SetStringField(TEXT("sku_kind"), SkuKind);
	Body->SetStringField(TEXT("sku_id"), SkuId);
	Body->SetStringField(TEXT("provider"), ResolvedProvider);
	Body->SetStringField(TEXT("purchase_token"), Token);
	const FString ProductId = StoreProductId.TrimStartAndEnd();
	if (!ProductId.IsEmpty())
	{
		Body->SetStringField(TEXT("store_product_id"), ProductId);
	}
	Http->PostJson(TEXT("/apps/games/shop/unlock/restore"), FastGameJsonUtil::Stringify(Body),
		[OnDone](bool bOk, int32 Code, FString Resp, FString Err)
		{
			if (!bOk)
			{
				int32 ParsedCode = Code;
				FString Msg;
				FFastGameHttp::ParseStatusFromError(bOk, Err, ParsedCode, Msg);
				if (OnDone) OnDone(false, false, Msg.IsEmpty() ? Err : Msg);
				return;
			}
			bool bSuccess = false;
			bool bOwned = false;
			const TSharedPtr<FJsonObject> Obj = FastGameJsonUtil::ParseObject(Resp);
			if (Obj.IsValid())
			{
				Obj->TryGetBoolField(TEXT("success"), bSuccess);
				Obj->TryGetBoolField(TEXT("owned"), bOwned);
			}
			const bool bUnlocked = bOwned || bSuccess;
			if (bUnlocked)
			{
				if (OnDone) OnDone(true, true, TEXT(""));
				return;
			}
			const FString ClientMsg = FFastGameHttp::ExtractShopUnlockMessage(
				Resp, TEXT("Store restore validation failed"));
			if (OnDone) OnDone(true, false, ClientMsg);
		});
}

void FFastGameShop::Buy(const FFastGameShopLine& Line, const FString& CallbackUrl,
	TFunction<void(bool, FFastGamePaymentInitiate, FString)> OnDone)
{
	BuyWithProvider(Line, CallbackUrl, TEXT("zarinpal"), TEXT("rial"), TEXT(""), OnDone);
}

void FFastGameShop::BuyWithProvider(const FFastGameShopLine& Line, const FString& CallbackUrl,
	const FString& Provider, const FString& Currency,
	TFunction<void(bool, FFastGamePaymentInitiate, FString)> OnDone)
{
	BuyWithProvider(Line, CallbackUrl, Provider, Currency, TEXT(""), OnDone);
}

void FFastGameShop::BuyWithProvider(const FFastGameShopLine& Line, const FString& CallbackUrl,
	const FString& Provider, const FString& Currency, const FString& DiscountCode,
	TFunction<void(bool, FFastGamePaymentInitiate, FString)> OnDone)
{
	FFastGamePaymentInitiate Empty;
	FString ResolvedProvider;
	FString ProvErr;
	if (!ResolveProvider(Provider, ResolvedProvider, ProvErr))
	{
		if (OnDone) OnDone(false, Empty, ProvErr);
		return;
	}
	FString ResolvedGame;
	FString GameErr;
	if (!ResolveGameCode(Line.GameCode, ResolvedGame, GameErr))
	{
		if (OnDone) OnDone(false, Empty, GameErr);
		return;
	}
	if (Line.bOwned)
	{
		if (OnDone) OnDone(false, Empty, TEXT("Already owned"));
		return;
	}
	// Price is resolved on the server from sku + provider + currency. Free items use ClaimFree.

	TSharedPtr<FJsonObject> CartLine = MakeShared<FJsonObject>();
	CartLine->SetStringField(TEXT("game_code"), ResolvedGame);
	CartLine->SetStringField(TEXT("sku_kind"), Line.SkuKind);
	CartLine->SetStringField(TEXT("sku_id"), Line.SkuId);
	TArray<TSharedPtr<FJsonValue>> Cart;
	Cart.Add(MakeShared<FJsonValueObject>(CartLine));

	TSharedPtr<FJsonObject> Meta = MakeShared<FJsonObject>();
	Meta->SetStringField(TEXT("kind"), TEXT("game_shop"));
	Meta->SetArrayField(TEXT("lines"), Cart);

	const bool bStore = ResolvedProvider.Equals(TEXT("myket")) || ResolvedProvider.Equals(TEXT("caffebazar")) || ResolvedProvider.Equals(TEXT("googleplay"));
	const bool bSteam = ResolvedProvider.Equals(TEXT("steam"));
	FString Path = TEXT("/base/payments/initiate");
	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("app_scope"), TEXT("fast-game"));
	Body->SetStringField(TEXT("purchase_type"), TEXT("one_time"));
	Body->SetStringField(TEXT("currency"), Currency.IsEmpty() ? TEXT("rial") : Currency);
	Body->SetObjectField(TEXT("metadata"), Meta);
	// Gateway description is optional; server builds it from sku_id NAMEs.

	if (bStore)
	{
		Path = TEXT("/base/payments/billing/initiate");
		Body->SetStringField(TEXT("provider"), ResolvedProvider);
	}
	else if (bSteam)
	{
		Path = TEXT("/base/payments/steam/initiate");
	}
	else
	{
		Body->SetStringField(TEXT("provider"), TEXT("zarinpal"));
		Body->SetStringField(TEXT("callback_url"), CallbackUrl);
	}
	if (!DiscountCode.IsEmpty() && !bStore)
	{
		Body->SetStringField(TEXT("discount_code"), DiscountCode);
	}

	const FString PendingSlot = Config.PendingPaymentSaveSlot;
	Http->PostJson(Path, FastGameJsonUtil::Stringify(Body),
		[Cart, ResolvedProvider, PendingSlot, OnDone](bool bOk, int32, FString Resp, FString Err)
		{
			FFastGamePaymentInitiate Result;
			Result.Provider = ResolvedProvider;
			if (!bOk)
			{
				if (OnDone) OnDone(false, Result, Err);
				return;
			}
			const TSharedPtr<FJsonObject> Obj = FastGameJsonUtil::ParseObject(Resp);
			if (Obj.IsValid())
			{
				Obj->TryGetStringField(TEXT("authority"), Result.Authority);
				Obj->TryGetStringField(TEXT("payment_url"), Result.PaymentUrl);
				Obj->TryGetStringField(TEXT("payment_token"), Result.PaymentToken);
				Obj->TryGetStringField(TEXT("store_product_id"), Result.StoreProductId);
				Obj->TryGetStringField(TEXT("orderid"), Result.OrderId);
				double Amount = 0;
				Obj->TryGetNumberField(TEXT("amount"), Amount);
				Result.Amount = static_cast<int32>(Amount);
			}
			TSharedPtr<FJsonObject> Pending = MakeShared<FJsonObject>();
			Pending->SetStringField(TEXT("authority"), Result.Authority);
			Pending->SetStringField(TEXT("payment_token"), Result.PaymentToken);
			Pending->SetStringField(TEXT("provider"), ResolvedProvider);
			Pending->SetArrayField(TEXT("lines"), Cart);
			const FString PendingPath = FastGameJsonUtil::PendingPath(PendingSlot);
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(PendingPath), true);
			FFileHelper::SaveStringToFile(FastGameJsonUtil::Stringify(Pending), *PendingPath);
			if (!Result.PaymentUrl.IsEmpty())
			{
				FPlatformProcess::LaunchURL(*Result.PaymentUrl, nullptr, nullptr);
			}
			if (OnDone) OnDone(true, Result, TEXT(""));
		});
}

void FFastGameShop::SubmitBilling(const FString& PurchaseToken, TFunction<void(bool, bool, FString)> OnDone)
{
	FString Raw;
	const FString Path = FastGameJsonUtil::PendingPath(Config.PendingPaymentSaveSlot);
	if (!FFileHelper::LoadFileToString(Raw, *Path))
	{
		if (OnDone) OnDone(false, false, TEXT("No pending payment"));
		return;
	}
	const TSharedPtr<FJsonObject> Pending = FastGameJsonUtil::ParseObject(Raw);
	if (!Pending.IsValid())
	{
		if (OnDone) OnDone(false, false, TEXT("Invalid pending payment"));
		return;
	}
	FString Authority, Token, Provider;
	Pending->TryGetStringField(TEXT("authority"), Authority);
	Pending->TryGetStringField(TEXT("payment_token"), Token);
	Pending->TryGetStringField(TEXT("provider"), Provider);
	TSharedPtr<FJsonObject> Meta = MakeShared<FJsonObject>();
	Meta->SetStringField(TEXT("kind"), TEXT("game_shop"));
	Meta->SetArrayField(TEXT("lines"), Pending->GetArrayField(TEXT("lines")));
	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("provider"), Provider);
	Body->SetStringField(TEXT("authority"), Authority);
	Body->SetStringField(TEXT("payment_token"), Token);
	Body->SetStringField(TEXT("purchase_token"), PurchaseToken);
	Body->SetObjectField(TEXT("metadata"), Meta);
	Http->PostJson(TEXT("/base/payments/billing/submit"), FastGameJsonUtil::Stringify(Body),
		[Path, OnDone](bool bOk, int32, FString Resp, FString Err)
		{
			if (!bOk)
			{
				if (OnDone) OnDone(false, false, Err);
				return;
			}
			bool bSuccess = false;
			const TSharedPtr<FJsonObject> Obj = FastGameJsonUtil::ParseObject(Resp);
			if (Obj.IsValid()) Obj->TryGetBoolField(TEXT("success"), bSuccess);
			IFileManager::Get().Delete(*Path);
			if (OnDone) OnDone(true, bSuccess, TEXT(""));
		});
}

void FFastGameShop::FinalizeSteam(TFunction<void(bool, bool, FString)> OnDone)
{
	FString Raw;
	const FString Path = FastGameJsonUtil::PendingPath(Config.PendingPaymentSaveSlot);
	if (!FFileHelper::LoadFileToString(Raw, *Path))
	{
		if (OnDone) OnDone(false, false, TEXT("No pending payment"));
		return;
	}
	const TSharedPtr<FJsonObject> Pending = FastGameJsonUtil::ParseObject(Raw);
	if (!Pending.IsValid())
	{
		if (OnDone) OnDone(false, false, TEXT("Invalid pending payment"));
		return;
	}
	FString Authority, Token;
	Pending->TryGetStringField(TEXT("authority"), Authority);
	Pending->TryGetStringField(TEXT("payment_token"), Token);
	TSharedPtr<FJsonObject> Meta = MakeShared<FJsonObject>();
	Meta->SetStringField(TEXT("kind"), TEXT("game_shop"));
	Meta->SetArrayField(TEXT("lines"), Pending->GetArrayField(TEXT("lines")));
	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("authority"), Authority);
	Body->SetStringField(TEXT("payment_token"), Token);
	Body->SetObjectField(TEXT("metadata"), Meta);
	Http->PostJson(TEXT("/base/payments/steam/finalize"), FastGameJsonUtil::Stringify(Body),
		[Path, OnDone](bool bOk, int32, FString Resp, FString Err)
		{
			if (!bOk)
			{
				if (OnDone) OnDone(false, false, Err);
				return;
			}
			bool bSuccess = false;
			const TSharedPtr<FJsonObject> Obj = FastGameJsonUtil::ParseObject(Resp);
			if (Obj.IsValid()) Obj->TryGetBoolField(TEXT("success"), bSuccess);
			IFileManager::Get().Delete(*Path);
			if (OnDone) OnDone(true, bSuccess, TEXT(""));
		});
}

void FFastGameShop::VerifyPending(const FString& AuthorityOverride, TFunction<void(bool, bool, FString)> OnDone)
{
	FString Raw;
	const FString Path = FastGameJsonUtil::PendingPath(Config.PendingPaymentSaveSlot);
	if (!FFileHelper::LoadFileToString(Raw, *Path))
	{
		if (OnDone) OnDone(false, false, TEXT("No pending payment"));
		return;
	}
	const TSharedPtr<FJsonObject> Pending = FastGameJsonUtil::ParseObject(Raw);
	if (!Pending.IsValid())
	{
		if (OnDone) OnDone(false, false, TEXT("Invalid pending payment"));
		return;
	}
	FString Provider;
	Pending->TryGetStringField(TEXT("provider"), Provider);
	if (Provider.Equals(TEXT("myket")) || Provider.Equals(TEXT("caffebazar")) || Provider.Equals(TEXT("googleplay")))
	{
		if (OnDone) OnDone(false, false, TEXT("Use SubmitBilling with the store purchase token"));
		return;
	}
	if (Provider.Equals(TEXT("steam")))
	{
		FinalizeSteam(OnDone);
		return;
	}
	FString Authority, Token;
	Pending->TryGetStringField(TEXT("authority"), Authority);
	Pending->TryGetStringField(TEXT("payment_token"), Token);
	if (!AuthorityOverride.IsEmpty()) Authority = AuthorityOverride;
	TSharedPtr<FJsonObject> Meta = MakeShared<FJsonObject>();
	Meta->SetStringField(TEXT("kind"), TEXT("game_shop"));
	Meta->SetArrayField(TEXT("lines"), Pending->GetArrayField(TEXT("lines")));
	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("provider"), TEXT("zarinpal"));
	Body->SetStringField(TEXT("authority"), Authority);
	Body->SetStringField(TEXT("payment_token"), Token);
	Body->SetObjectField(TEXT("metadata"), Meta);
	Http->PostJson(TEXT("/base/payments/verify"), FastGameJsonUtil::Stringify(Body),
		[Path, OnDone](bool bOk, int32, FString Resp, FString Err)
		{
			if (!bOk)
			{
				if (OnDone) OnDone(false, false, Err);
				return;
			}
			bool bSuccess = false;
			const TSharedPtr<FJsonObject> Obj = FastGameJsonUtil::ParseObject(Resp);
			if (Obj.IsValid()) Obj->TryGetBoolField(TEXT("success"), bSuccess);
			IFileManager::Get().Delete(*Path);
			if (OnDone) OnDone(true, bSuccess, TEXT(""));
		});
}

void FFastGameShop::ClearPendingPayment()
{
	IFileManager::Get().Delete(*FastGameJsonUtil::PendingPath(Config.PendingPaymentSaveSlot));
}

TArray<FFastGameAssetPack> FFastGameAssets::ListPacksFromGame(const FFastGameCatalogDetail& Detail)
{
	return Detail.AssetPacks;
}

FFastGameAssetPack FFastGameAssets::ParsePack(const TSharedPtr<FJsonObject>& PackObject)
{
	return FastGameJsonUtil::ParseAssetPack(PackObject);
}

TArray<FFastGameAssetPack> FFastGameAssets::ListPacksFromGameTip(const TSharedPtr<FJsonObject>& GameTip)
{
	TArray<FFastGameAssetPack> Out;
	if (!GameTip.IsValid())
	{
		return Out;
	}
	const TSharedPtr<FJsonObject>* PayloadPtr = nullptr;
	const TSharedPtr<FJsonObject> Source = GameTip->TryGetObjectField(TEXT("payload"), PayloadPtr) && PayloadPtr && PayloadPtr->IsValid()
		? *PayloadPtr
		: GameTip;
	const TArray<TSharedPtr<FJsonValue>>* Packs = nullptr;
	if (!Source->TryGetArrayField(TEXT("asset_packs"), Packs) || !Packs)
	{
		return Out;
	}
	for (const TSharedPtr<FJsonValue>& V : *Packs)
	{
		Out.Add(FastGameJsonUtil::ParseAssetPack(V->AsObject()));
	}
	return Out;
}

static FFastGameAdvertisement ParseAdvertisement(const TSharedPtr<FJsonObject>& O)
{
	FFastGameAdvertisement Ad;
	if (!O.IsValid()) return Ad;
	O->TryGetStringField(TEXT("id"), Ad.Id);
	O->TryGetStringField(TEXT("campaign_id"), Ad.CampaignId);
	const TSharedPtr<FJsonObject>* Media = nullptr;
	if (O->TryGetObjectField(TEXT("media"), Media) && Media && Media->IsValid())
	{
		(*Media)->TryGetStringField(TEXT("type"), Ad.MediaType);
		(*Media)->TryGetStringField(TEXT("url"), Ad.MediaUrl);
		double W = 0, H = 0;
		(*Media)->TryGetNumberField(TEXT("width"), W);
		(*Media)->TryGetNumberField(TEXT("height"), H);
		Ad.MediaWidth = static_cast<int32>(W);
		Ad.MediaHeight = static_cast<int32>(H);
	}
	const TSharedPtr<FJsonObject>* Click = nullptr;
	if (O->TryGetObjectField(TEXT("click"), Click) && Click && Click->IsValid())
	{
		(*Click)->TryGetBoolField(TEXT("enabled"), Ad.bClickEnabled);
		(*Click)->TryGetStringField(TEXT("url"), Ad.ClickUrl);
	}
	const TSharedPtr<FJsonObject>* Tracking = nullptr;
	if (O->TryGetObjectField(TEXT("tracking"), Tracking) && Tracking && Tracking->IsValid())
	{
		(*Tracking)->TryGetStringField(TEXT("impression_url"), Ad.ImpressionTrackingUrl);
		(*Tracking)->TryGetStringField(TEXT("click_url"), Ad.ClickTrackingUrl);
	}
	const TSharedPtr<FJsonObject>* Meta = nullptr;
	if (O->TryGetObjectField(TEXT("meta"), Meta) && Meta && Meta->IsValid())
	{
		Ad.Meta = *Meta;
		(*Meta)->TryGetStringField(TEXT("title"), Ad.Title);
		(*Meta)->TryGetStringField(TEXT("body"), Ad.Body);
		(*Meta)->TryGetStringField(TEXT("background_url"), Ad.BackgroundUrl);
		(*Meta)->TryGetStringField(TEXT("background_color"), Ad.BackgroundColor);
	}
	return Ad;
}

void FFastGameAds::GetAdvertisement(const FFastGameAdvertisementRequest& Request,
	TFunction<void(bool, bool, FFastGameAdvertisement, FString)> OnDone)
{
	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("game_id"), Request.GameId);
	if (!Request.Slot.IsEmpty()) Body->SetStringField(TEXT("slot"), Request.Slot);
	if (!Request.MediaType.IsEmpty()) Body->SetStringField(TEXT("media_type"), Request.MediaType);
	if (!Request.Format.IsEmpty()) Body->SetStringField(TEXT("format"), Request.Format);
	TArray<TSharedPtr<FJsonValue>> Tags;
	for (const FString& Tag : Request.Tags)
	{
		Tags.Add(MakeShared<FJsonValueString>(Tag));
	}
	Body->SetArrayField(TEXT("tags"), Tags);
	if (!Request.Locale.IsEmpty()) Body->SetStringField(TEXT("locale"), Request.Locale);
	if (!Request.Country.IsEmpty()) Body->SetStringField(TEXT("country"), Request.Country);
	if (!Request.Platform.IsEmpty()) Body->SetStringField(TEXT("platform"), Request.Platform);
	if (!Request.Engine.IsEmpty()) Body->SetStringField(TEXT("engine"), Request.Engine);
	if (Request.Capabilities.IsValid())
	{
		Body->SetObjectField(TEXT("capabilities"), Request.Capabilities);
	}
	else
	{
		Body->SetObjectField(TEXT("capabilities"), MakeShared<FJsonObject>());
	}
	Http->PostJson(TEXT("/apps/games/ads/request"), FastGameJsonUtil::Stringify(Body),
		[OnDone](bool bOk, int32 Code, FString Resp, FString Err)
		{
			if (!bOk)
			{
				if (OnDone) OnDone(false, false, FFastGameAdvertisement(), Err);
				return;
			}
			if (Code == 204 || Resp.IsEmpty())
			{
				if (OnDone) OnDone(true, false, FFastGameAdvertisement(), TEXT(""));
				return;
			}
			const TSharedPtr<FJsonObject> Obj = FastGameJsonUtil::ParseObject(Resp);
			if (!Obj.IsValid())
			{
				if (OnDone) OnDone(false, false, FFastGameAdvertisement(), TEXT("Invalid advertisement response"));
				return;
			}
			if (OnDone) OnDone(true, true, ParseAdvertisement(Obj), TEXT(""));
		});
}

void FFastGameAds::TrackEvent(const FFastGameAdvertisementEvent& Event,
	TFunction<void(bool, FString)> OnDone)
{
	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("event_type"), Event.EventType);
	if (!Event.AdId.IsEmpty()) Body->SetStringField(TEXT("ad_id"), Event.AdId);
	if (!Event.GameId.IsEmpty()) Body->SetStringField(TEXT("game_id"), Event.GameId);
	if (!Event.CampaignId.IsEmpty()) Body->SetStringField(TEXT("campaign_id"), Event.CampaignId);
	if (!Event.Timestamp.IsEmpty()) Body->SetStringField(TEXT("timestamp"), Event.Timestamp);
	if (Event.Extras.IsValid())
	{
		Body->SetObjectField(TEXT("extras"), Event.Extras);
	}
	else
	{
		Body->SetObjectField(TEXT("extras"), MakeShared<FJsonObject>());
	}
	Http->PostJson(TEXT("/apps/games/ads/events"), FastGameJsonUtil::Stringify(Body),
		[OnDone](bool bOk, int32, FString, FString Err)
		{
			if (OnDone) OnDone(bOk, bOk ? TEXT("") : Err);
		});
}
