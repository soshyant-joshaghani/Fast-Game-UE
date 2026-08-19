#include "FastGameStoreSubsystem.h"
#include "FastGameNativeStore.h"
#include "FastGameTypes.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Async/Async.h"
#include "HAL/PlatformTime.h"

#if PLATFORM_ANDROID
#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#endif

namespace FastGameStoreNative
{
	FString CompiledFlavor()
	{
#if defined(FASTGAME_STORE_CAFEBAZAAR)
		return TEXT("caffebazar");
#elif defined(FASTGAME_STORE_GOOGLEPLAY)
		return TEXT("googleplay");
#else
		return TEXT("myket");
#endif
	}

	const char* FlavorPackage()
	{
#if defined(FASTGAME_STORE_CAFEBAZAAR)
		return "com.farsitel.bazaar";
#elif defined(FASTGAME_STORE_GOOGLEPLAY)
		return "com.android.vending";
#else
		return "ir.mservices.market";
#endif
	}

	bool IsPackageInstalled(const char* PackageName)
	{
#if PLATFORM_ANDROID
		JNIEnv* Env = FAndroidApplication::GetJavaEnv();
		if (!Env)
		{
			return false;
		}
		jobject Activity = FAndroidApplication::GetGameActivityThis();
		if (!Activity)
		{
			return false;
		}
		jclass ActivityClazz = Env->GetObjectClass(Activity);
		jmethodID GetPM = Env->GetMethodID(ActivityClazz, "getPackageManager", "()Landroid/content/pm/PackageManager;");
		if (!GetPM)
		{
			Env->DeleteLocalRef(ActivityClazz);
			return false;
		}
		jobject PM = Env->CallObjectMethod(Activity, GetPM);
		jclass PMClass = Env->GetObjectClass(PM);
		jmethodID GetInfo = Env->GetMethodID(PMClass, "getPackageInfo", "(Ljava/lang/String;I)Landroid/content/pm/PackageInfo;");
		jstring Name = Env->NewStringUTF(PackageName);
		jobject Info = Env->CallObjectMethod(PM, GetInfo, Name, 0);
		const bool bException = Env->ExceptionCheck() == JNI_TRUE;
		if (bException)
		{
			Env->ExceptionClear();
		}
		Env->DeleteLocalRef(Name);
		Env->DeleteLocalRef(PMClass);
		Env->DeleteLocalRef(PM);
		Env->DeleteLocalRef(ActivityClazz);
		if (bException || !Info)
		{
			return false;
		}
		Env->DeleteLocalRef(Info);
		return true;
#else
		(void)PackageName;
		return false;
#endif
	}

	bool LaunchActivity(bool bOpenStorePage, const FString& StoreProductId, const FString& StorePublicKey)
	{
#if PLATFORM_ANDROID
		JNIEnv* Env = FAndroidApplication::GetJavaEnv();
		if (!Env)
		{
			return false;
		}
		jobject Activity = FAndroidApplication::GetGameActivityThis();
		if (!Activity)
		{
			return false;
		}
		jclass ActivityClazz = Env->GetObjectClass(Activity);
		const FTCHARToUTF8 ProductUtf8(*StoreProductId);
		const FTCHARToUTF8 KeyUtf8(*StorePublicKey);
		jstring ProductVal = Env->NewStringUTF(ProductUtf8.Get());
		jstring KeyVal = Env->NewStringUTF(KeyUtf8.Get());
		UE_LOG(
			LogTemp,
			Log,
			TEXT("FastGameStore: launch open=%s sku='%s' rsaChars=%d"),
			bOpenStorePage ? TEXT("true") : TEXT("false"),
			*StoreProductId,
			StorePublicKey.Len());

		jmethodID Launch = Env->GetMethodID(
			ActivityClazz, "LaunchFastGameStore", "(ZLjava/lang/String;Ljava/lang/String;)V");
		if (Launch)
		{
			Env->CallVoidMethod(
				Activity, Launch, bOpenStorePage ? JNI_TRUE : JNI_FALSE, ProductVal, KeyVal);
		}
		else
		{
			Env->ExceptionClear();
			jclass ActivityClass = FAndroidApplication::FindJavaClassGlobalRef("com/fastgame/store/FastGameStoreActivity");
			if (!ActivityClass)
			{
				UE_LOG(LogTemp, Error, TEXT("FastGameStore: FastGameStoreActivity class not found"));
				Env->DeleteLocalRef(ProductVal);
				Env->DeleteLocalRef(KeyVal);
				Env->DeleteLocalRef(ActivityClazz);
				return false;
			}
			jmethodID StartActivityMethod = Env->GetMethodID(ActivityClazz, "startActivity", "(Landroid/content/Intent;)V");
			jclass IntentClass = Env->FindClass("android/content/Intent");
			jmethodID IntentCtor = Env->GetMethodID(IntentClass, "<init>", "(Landroid/content/Context;Ljava/lang/Class;)V");
			jobject Intent = Env->NewObject(IntentClass, IntentCtor, Activity, ActivityClass);
			jmethodID PutExtraBool = Env->GetMethodID(IntentClass, "putExtra", "(Ljava/lang/String;Z)Landroid/content/Intent;");
			jmethodID PutExtraStr = Env->GetMethodID(
				IntentClass, "putExtra", "(Ljava/lang/String;Ljava/lang/String;)Landroid/content/Intent;");
			jstring OpenKey = Env->NewStringUTF("openTheStorePage");
			Env->CallObjectMethod(Intent, PutExtraBool, OpenKey, bOpenStorePage ? JNI_TRUE : JNI_FALSE);
			Env->DeleteLocalRef(OpenKey);
			jstring ProductKey = Env->NewStringUTF("storeProductId");
			Env->CallObjectMethod(Intent, PutExtraStr, ProductKey, ProductVal);
			Env->DeleteLocalRef(ProductKey);
			jstring KeyName = Env->NewStringUTF("storePublicKey");
			Env->CallObjectMethod(Intent, PutExtraStr, KeyName, KeyVal);
			Env->DeleteLocalRef(KeyName);
			Env->CallVoidMethod(Activity, StartActivityMethod, Intent);
			Env->DeleteLocalRef(Intent);
			Env->DeleteLocalRef(IntentClass);
		}

		Env->DeleteLocalRef(ProductVal);
		Env->DeleteLocalRef(KeyVal);
		Env->DeleteLocalRef(ActivityClazz);
		if (Env->ExceptionCheck())
		{
			Env->ExceptionDescribe();
			Env->ExceptionClear();
			return false;
		}
		return true;
#else
		(void)bOpenStorePage;
		(void)StoreProductId;
		(void)StorePublicKey;
		return false;
#endif
	}
}

void UFastGameStoreSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FFastGameNativeStore::Register(this);
}

void UFastGameStoreSubsystem::Deinitialize()
{
	FFastGameNativeStore::Unregister(this);
	Super::Deinitialize();
}

void UFastGameStoreSubsystem::SetStorePublicKey(const FString& PublicKey)
{
	StorePublicKey = PublicKey.TrimStartAndEnd();
}

FString UFastGameStoreSubsystem::GetCompiledStoreFlavor() const
{
	return FastGameStoreNative::CompiledFlavor();
}

bool UFastGameStoreSubsystem::IsStoreAppInstalled() const
{
	return FastGameStoreNative::IsPackageInstalled(FastGameStoreNative::FlavorPackage());
}

bool UFastGameStoreSubsystem::LaunchStoreActivity(bool bOpenStorePage, const FString& StoreProductId)
{
	return FastGameStoreNative::LaunchActivity(bOpenStorePage, StoreProductId, StorePublicKey);
}

bool UFastGameStoreSubsystem::EnsureSetup(const FString& ProviderId, const FString& PublicKey, FString& OutMessage)
{
	if (!PublicKey.TrimStartAndEnd().IsEmpty())
	{
		SetStorePublicKey(PublicKey);
	}
	const FString Provider = FastGameNormalizeProviderId(ProviderId);
	if (!FFastGameNativeStore::IsAndroidStoreProvider(Provider))
	{
		OutMessage.Empty();
		return true;
	}
#if !PLATFORM_ANDROID
	OutMessage = TEXT("FastGameStore: editor/non-Android — install check skipped");
	return true;
#else
	const FString Flavor = GetCompiledStoreFlavor();
	if (!Provider.Equals(Flavor, ESearchCase::IgnoreCase))
	{
		OutMessage = FString::Printf(
			TEXT("FastGameStore: APK flavor '%s' does not match StorePlatform '%s'"), *Flavor, *Provider);
		return false;
	}
	if (!IsStoreAppInstalled())
	{
		OutMessage = FastGameStoreNotInstalledMessage(Provider);
		return false;
	}
	OutMessage.Empty();
	return true;
#endif
}

void UFastGameStoreSubsystem::RequestPurchaseToken(const FString& StoreProductId, TFunction<void(FString, bool)> OnDone)
{
	LaunchNativePurchase(StoreProductId, true, MoveTemp(OnDone));
}

void UFastGameStoreSubsystem::QueryStoreOwnership(const FString& StoreProductId, TFunction<void(FString, bool)> OnDone)
{
	LaunchNativePurchase(StoreProductId, false, MoveTemp(OnDone));
}

void UFastGameStoreSubsystem::LaunchNativePurchase(
	const FString& StoreProductId,
	bool bOpenStorePage,
	TFunction<void(FString, bool)> OnDone)
{
	if (StoreProductId.TrimStartAndEnd().IsEmpty())
	{
		if (OnDone) OnDone(TEXT(""), false);
		return;
	}
#if !PLATFORM_ANDROID
	if (OnDone) OnDone(TEXT(""), false);
	return;
#else
	constexpr double QueryCacheSeconds = 45.0;
	const double Now = FPlatformTime::Seconds();
	if (bOpenStorePage)
	{
		CachedQueryAt = 0.0;
		CachedQueryToken.Empty();
		CachedQueryOwned = false;
	}
	if (!bOpenStorePage
		&& CachedQuerySku.Equals(StoreProductId, ESearchCase::IgnoreCase)
		&& (Now - CachedQueryAt) < QueryCacheSeconds)
	{
		UE_LOG(LogTemp, Log, TEXT("FastGameStore: reuse inventory cache sku='%s'"), *StoreProductId);
		if (OnDone)
		{
			OnDone(CachedQueryToken, CachedQueryOwned);
		}
		return;
	}
	if (bActivityOpen)
	{
		if (bOpenStorePage)
		{
			UE_LOG(LogTemp, Warning, TEXT("FastGameStore: queue purchase until current activity ends sku='%s'"), *StoreProductId);
			bQueuedOpenStore = true;
			QueuedOpenStoreSku = StoreProductId;
			QueuedOpenStoreDone = MoveTemp(OnDone);
			return;
		}
		UE_LOG(LogTemp, Log, TEXT("FastGameStore: skip overlapping inventory query sku='%s'"), *StoreProductId);
		if (OnDone)
		{
			const bool bSame = CachedQuerySku.Equals(StoreProductId, ESearchCase::IgnoreCase);
			OnDone(bSame ? CachedQueryToken : FString(), bSame && CachedQueryOwned);
		}
		return;
	}

	PendingNative = MoveTemp(OnDone);
	bActivityOpen = true;
	if (!LaunchStoreActivity(bOpenStorePage, StoreProductId))
	{
		bActivityOpen = false;
		TFunction<void(FString, bool)> Cb = MoveTemp(PendingNative);
		PendingNative = nullptr;
		if (Cb) Cb(TEXT(""), false);
	}
#endif
}

void UFastGameStoreSubsystem::BroadcastStorePurchase(FString StoreProductId, FString PurchaseToken, bool bAlreadyOwned)
{
	CachedQuerySku = StoreProductId;
	CachedQueryToken = PurchaseToken;
	CachedQueryOwned = bAlreadyOwned || !PurchaseToken.TrimStartAndEnd().IsEmpty();
	CachedQueryAt = FPlatformTime::Seconds();
	bActivityOpen = false;

	OnStorePurchase.Broadcast(StoreProductId, PurchaseToken, bAlreadyOwned);
	if (PendingNative)
	{
		TFunction<void(FString, bool)> Cb = MoveTemp(PendingNative);
		PendingNative = nullptr;
		Cb(PurchaseToken, bAlreadyOwned);
	}

	if (bQueuedOpenStore)
	{
		bQueuedOpenStore = false;
		const FString Sku = MoveTemp(QueuedOpenStoreSku);
		TFunction<void(FString, bool)> Queued = MoveTemp(QueuedOpenStoreDone);
		QueuedOpenStoreDone = nullptr;
		LaunchNativePurchase(Sku, true, MoveTemp(Queued));
	}
}

#if PLATFORM_ANDROID
extern "C" JNIEXPORT void JNICALL
Java_com_fastgame_store_FastGameStoreActivity_OnStorePurchase(
	JNIEnv* Env,
	jobject /*Thiz*/,
	jstring StoreProductId,
	jstring PurchaseToken,
	jboolean AlreadyOwned)
{
	FString Sku;
	FString Token;
	if (Env && StoreProductId)
	{
		const char* Chars = Env->GetStringUTFChars(StoreProductId, nullptr);
		if (Chars)
		{
			Sku = UTF8_TO_TCHAR(Chars);
			Env->ReleaseStringUTFChars(StoreProductId, Chars);
		}
	}
	if (Env && PurchaseToken)
	{
		const char* Chars = Env->GetStringUTFChars(PurchaseToken, nullptr);
		if (Chars)
		{
			Token = UTF8_TO_TCHAR(Chars);
			Env->ReleaseStringUTFChars(PurchaseToken, Chars);
		}
	}
	const bool bOwned = AlreadyOwned == JNI_TRUE;
	AsyncTask(ENamedThreads::GameThread, [Sku, Token, bOwned]()
	{
		if (!GEngine)
		{
			return;
		}
		for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		{
			if (UWorld* World = Ctx.World())
			{
				if (UGameInstance* GI = World->GetGameInstance())
				{
					if (UFastGameStoreSubsystem* Store = GI->GetSubsystem<UFastGameStoreSubsystem>())
					{
						Store->BroadcastStorePurchase(Sku, Token, bOwned);
						return;
					}
				}
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("FastGameStore: OnStorePurchase — no FastGameStoreSubsystem"));
	});
}
#endif
