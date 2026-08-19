#include "FastGameNativeStore.h"
#include "FastGameTypes.h"

namespace
{
	IFastGameNativeStore* GNativeStore = nullptr;
}

void FFastGameNativeStore::Register(IFastGameNativeStore* Impl)
{
	GNativeStore = Impl;
}

void FFastGameNativeStore::Unregister(IFastGameNativeStore* Impl)
{
	if (GNativeStore == Impl)
	{
		GNativeStore = nullptr;
	}
}

IFastGameNativeStore* FFastGameNativeStore::Get()
{
	return GNativeStore;
}

bool FFastGameNativeStore::IsAndroidStoreProvider(const FString& ProviderId)
{
	const FString Id = FastGameNormalizeProviderId(ProviderId);
	return Id.Equals(TEXT("myket"), ESearchCase::IgnoreCase)
		|| Id.Equals(TEXT("caffebazar"), ESearchCase::IgnoreCase)
		|| Id.Equals(TEXT("googleplay"), ESearchCase::IgnoreCase);
}
