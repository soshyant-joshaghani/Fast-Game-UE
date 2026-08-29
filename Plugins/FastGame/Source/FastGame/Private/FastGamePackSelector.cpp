#include "FastGamePackSelector.h"

TArray<FFastGameAssetPack> FFastGamePackSelector::ListForDownload(
	const TArray<FFastGameAssetPack>& Index,
	const FFastGameDownloadContext& Context)
{
	TArray<FFastGameAssetPack> Out;
	for (const FFastGameAssetPack& Pack : Index)
	{
		if (Context.bSkipSplashPacks && IsSplashPack(Pack))
		{
			continue;
		}
		if (!MatchesTagList(Pack.Quality, Context.QualityClass))
		{
			continue;
		}
		if (!MatchesTagList(Pack.Platforms, Context.RuntimeOs))
		{
			continue;
		}
		if (!MatchesTagList(Pack.Languages, Context.PreferredLanguage))
		{
			continue;
		}
		Out.Add(Pack);
	}
	return Out;
}

bool FFastGamePackSelector::MatchesTagList(const TArray<FString>& Tags, const FString& Value)
{
	if (Tags.Num() == 0)
	{
		return true;
	}
	if (Value.IsEmpty())
	{
		return false;
	}
	for (const FString& Tag : Tags)
	{
		if (Tag.IsEmpty())
		{
			continue;
		}
		if (Tag == TEXT("*") || Tag.Equals(Value, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

bool FFastGamePackSelector::IsSplashPack(const FFastGameAssetPack& Pack)
{
	return Pack.PackId.Equals(TEXT("splash"), ESearchCase::IgnoreCase)
		|| Pack.Kind.Equals(TEXT("splash"), ESearchCase::IgnoreCase);
}
