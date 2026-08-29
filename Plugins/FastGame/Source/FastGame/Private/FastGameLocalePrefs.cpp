#include "FastGameLocalePrefs.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	FString LocalePath()
	{
		return FPaths::ProjectSavedDir() / TEXT("FastGame") / TEXT("preferred_language.txt");
	}
}

FString FFastGameLocalePrefs::Get(const FString& Fallback)
{
	FString Lang;
	if (FFileHelper::LoadFileToString(Lang, *LocalePath()))
	{
		Lang.TrimStartAndEndInline();
		if (!Lang.IsEmpty())
		{
			return Lang.ToLower();
		}
	}
	return Fallback.IsEmpty() ? TEXT("en") : Fallback.ToLower();
}

void FFastGameLocalePrefs::Set(const FString& Language)
{
	if (Language.IsEmpty())
	{
		return;
	}
	IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir() / TEXT("FastGame")), true);
	FFileHelper::SaveStringToFile(Language.TrimStartAndEnd().ToLower(), *LocalePath());
}

void FFastGameLocalePrefs::Clear()
{
	IFileManager::Get().Delete(*LocalePath());
}
