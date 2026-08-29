#pragma once

#include "CoreMinimal.h"
#include "FastGameTypes.h"

struct FASTGAME_API FFastGameDownloadContext
{
	FString QualityClass = TEXT("mobile");
	FString RuntimeOs = TEXT("android");
	FString PreferredLanguage = TEXT("en");
	bool bSkipSplashPacks = true;
};

/** Filter published tip pack index for DOWNLOAD (quality × platform × language). */
class FASTGAME_API FFastGamePackSelector
{
public:
	static TArray<FFastGameAssetPack> ListForDownload(
		const TArray<FFastGameAssetPack>& Index,
		const FFastGameDownloadContext& Context);

	static bool MatchesTagList(const TArray<FString>& Tags, const FString& Value);

private:
	static bool IsSplashPack(const FFastGameAssetPack& Pack);
};
