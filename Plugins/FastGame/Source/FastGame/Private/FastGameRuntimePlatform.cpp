#include "FastGameRuntimePlatform.h"
#include "FastGameTypes.h"

FString FFastGameRuntimePlatform::GetRuntimeOs()
{
#if PLATFORM_ANDROID
	return TEXT("android");
#elif PLATFORM_IOS
	return TEXT("ios");
#elif PLATFORM_MAC
	return TEXT("mac");
#elif PLATFORM_WINDOWS
	return TEXT("windows");
#elif PLATFORM_LINUX
	return TEXT("windows");
#else
	return TEXT("windows");
#endif
}

FString FFastGameRuntimePlatform::GetQualityClass(const FString& RuntimeOs)
{
	const FString Os = RuntimeOs.IsEmpty() ? GetRuntimeOs() : RuntimeOs;
	return (Os.Equals(TEXT("windows"), ESearchCase::IgnoreCase)
		|| Os.Equals(TEXT("mac"), ESearchCase::IgnoreCase))
		? TEXT("pc")
		: TEXT("mobile");
}

FString FFastGameRuntimePlatform::StorePlatformToOs(const FString& StorePlatform)
{
	const FString Id = FastGameNormalizeProviderId(StorePlatform);
	if (Id == TEXT("myket") || Id == TEXT("caffebazar") || Id == TEXT("googleplay"))
	{
		return TEXT("android");
	}
	if (Id == TEXT("steam"))
	{
		return TEXT("windows");
	}
	if (Id == TEXT("appstore"))
	{
		return TEXT("ios");
	}
	if (Id == TEXT("macstore"))
	{
		return TEXT("mac");
	}
	return FString();
}
