#include "FastGamePackDownload.h"
#include "FastGameClient.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

bool FFastGamePackDownload::HasDownloadUrl(const FFastGameAssetPack& Pack)
{
	return !Pack.Url.IsEmpty();
}

FString FFastGamePackDownload::PickPartUrl(const TSharedPtr<FJsonObject>& PackTip)
{
	if (!PackTip.IsValid())
	{
		return FString();
	}
	const TArray<TSharedPtr<FJsonValue>>* Parts = nullptr;
	if (!PackTip->TryGetArrayField(TEXT("parts"), Parts) || !Parts)
	{
		return FString();
	}

	FString ZipUrl;
	FString PackUrl;
	FString AnyUrl;
	for (const TSharedPtr<FJsonValue>& Item : *Parts)
	{
		const TSharedPtr<FJsonObject> Part = Item->AsObject();
		if (!Part.IsValid())
		{
			continue;
		}
		FString Status;
		Part->TryGetStringField(TEXT("status"), Status);
		if (!Status.Equals(TEXT("ready"), ESearchCase::IgnoreCase))
		{
			continue;
		}
		FString Url;
		Part->TryGetStringField(TEXT("public_url"), Url);
		if (Url.IsEmpty())
		{
			continue;
		}
		if (AnyUrl.IsEmpty())
		{
			AnyUrl = Url;
		}
		FString PartId;
		FString Kind;
		Part->TryGetStringField(TEXT("part_id"), PartId);
		Part->TryGetStringField(TEXT("kind"), Kind);
		if (PartId.Equals(TEXT("pack"), ESearchCase::IgnoreCase))
		{
			PackUrl = Url;
		}
		if (Kind.Equals(TEXT("zip"), ESearchCase::IgnoreCase))
		{
			ZipUrl = Url;
		}
	}
	if (!PackUrl.IsEmpty())
	{
		return PackUrl;
	}
	if (!ZipUrl.IsEmpty())
	{
		return ZipUrl;
	}
	return AnyUrl;
}

void FFastGamePackDownload::ResolveDownloadUrl(
	TSharedRef<FFastGameContent> Content,
	const FString& GameCode,
	FFastGameAssetPack& Pack,
	TFunction<void(bool, const FString&, FString)> OnDone)
{
	if (!Pack.Url.IsEmpty())
	{
		if (OnDone)
		{
			OnDone(true, Pack.Url, TEXT(""));
		}
		return;
	}
	Content->GetPackTip(GameCode, Pack.PackId,
		[&Pack, OnDone = MoveTemp(OnDone)](bool bOk, TSharedPtr<FJsonObject> Tip, FString Error)
		{
			if (!bOk)
			{
				if (OnDone)
				{
					OnDone(false, FString(), Error);
				}
				return;
			}
			const FString Url = PickPartUrl(Tip);
			if (!Url.IsEmpty())
			{
				Pack.Url = Url;
			}
			if (OnDone)
			{
				OnDone(!Url.IsEmpty(), Url, Url.IsEmpty() ? TEXT("no ready part url") : FString());
			}
		});
}

void FFastGamePackDownload::DownloadToCache(
	const FFastGameAssetPack& Pack,
	const FString& Url,
	TFunction<void(bool, FString)> OnDone)
{
	if (Url.IsEmpty())
	{
		if (OnDone)
		{
			OnDone(false, TEXT("empty url"));
		}
		return;
	}

	const FString Dir = FPaths::ProjectSavedDir() / TEXT("FastGame/packs")
		/ SanitizeFileName(Pack.PackId.IsEmpty() ? Pack.Id : Pack.PackId);
	IFileManager::Get().MakeDirectory(*Dir, true);

	const FString FileName = SanitizeFileName(
		!Pack.Hash.IsEmpty() ? Pack.Hash : FString::Printf(TEXT("rev_%d"), FMath::Max(1, Pack.Revision)));
	const FString Path = Dir / FileName;
	if (FPaths::FileExists(Path))
	{
		if (OnDone)
		{
			OnDone(true, TEXT(""));
		}
		return;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(Url);
	Req->SetVerb(TEXT("GET"));
	Req->OnProcessRequestComplete().BindLambda(
		[Path, OnDone = MoveTemp(OnDone)](
			FHttpRequestPtr,
			FHttpResponsePtr Response,
			bool bConnected)
		{
			if (!bConnected || !Response.IsValid() || !EHttpResponseCodes::IsOk(Response->GetResponseCode()))
			{
				if (OnDone)
				{
					OnDone(false, TEXT("download failed"));
				}
				return;
			}
			if (!FFileHelper::SaveArrayToFile(Response->GetContent(), *Path))
			{
				if (OnDone)
				{
					OnDone(false, TEXT("write failed"));
				}
				return;
			}
			if (OnDone)
			{
				OnDone(true, TEXT(""));
			}
		});
	Req->ProcessRequest();
}

FString FFastGamePackDownload::SanitizeFileName(FString Value)
{
	if (Value.IsEmpty())
	{
		return TEXT("file");
	}
	Value.ReplaceInline(TEXT("\\"), TEXT("_"));
	Value.ReplaceInline(TEXT("/"), TEXT("_"));
	Value.ReplaceInline(TEXT(":"), TEXT("_"));
	Value.ReplaceInline(TEXT("*"), TEXT("_"));
	Value.ReplaceInline(TEXT("?"), TEXT("_"));
	Value.ReplaceInline(TEXT("\""), TEXT("_"));
	Value.ReplaceInline(TEXT("<"), TEXT("_"));
	Value.ReplaceInline(TEXT(">"), TEXT("_"));
	Value.ReplaceInline(TEXT("|"), TEXT("_"));
	return Value;
}
