#pragma once

#include "CoreMinimal.h"

/** Guest / client preferred language until synced after AUTH (B1a). */
class FASTGAME_API FFastGameLocalePrefs
{
public:
	static FString Get(const FString& Fallback = TEXT("en"));
	static void Set(const FString& Language);
	static void Clear();
};
