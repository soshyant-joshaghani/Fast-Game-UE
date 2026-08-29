#pragma once

#include "CoreMinimal.h"
#include "FastGameTypes.h"

class FFastGameContent;

/** Resolve pack URLs from tip index or pack tip parts; write to Saved/FastGame/packs. */
class FASTGAME_API FFastGamePackDownload
{
public:
	static bool HasDownloadUrl(const FFastGameAssetPack& Pack);
	static FString PickPartUrl(const TSharedPtr<FJsonObject>& PackTip);
	static void ResolveDownloadUrl(
		TSharedRef<FFastGameContent> Content,
		const FString& GameCode,
		FFastGameAssetPack& Pack,
		TFunction<void(bool /*bOk*/, const FString& /*Url*/, FString /*Error*/)> OnDone);
	static void DownloadToCache(
		const FFastGameAssetPack& Pack,
		const FString& Url,
		TFunction<void(bool /*bOk*/, FString /*Error*/)> OnDone);

private:
	static FString SanitizeFileName(FString Value);
};
