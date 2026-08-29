#pragma once

#include "CoreMinimal.h"
#include "FastGame.h"

/** Runtime OS + quality class for DOWNLOAD pack filtering (cross-platform §2). */
struct FASTGAME_API FFastGameRuntimePlatform
{
	static FString GetRuntimeOs();
	static FString GetQualityClass(const FString& RuntimeOs = FString());
	/** Store flavor hint for editor smoke (pack platforms[] still use OS ids). */
	static FString StorePlatformToOs(const FString& StorePlatform);
};
