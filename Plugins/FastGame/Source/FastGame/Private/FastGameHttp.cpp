#include "FastGameHttp.h"
#include "HttpModule.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace FastGameHttpJsonUtil
{
	static TSharedPtr<FJsonObject> ParseObject(const FString& Text)
	{
		TSharedPtr<FJsonValue> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid() || Root->Type != EJson::Object)
		{
			return nullptr;
		}
		return Root->AsObject();
	}
}

namespace FastGameHttpUtil
{
	/** Accept host-only values like "api.localhost" and normalize to http://api.localhost/api/v1. */
	static FString NormalizeApiBaseUrl(FString Url)
	{
		Url.TrimStartAndEndInline();
		if (Url.IsEmpty())
		{
			return TEXT("http://api.localhost/api/v1");
		}
		if (!Url.StartsWith(TEXT("http://"), ESearchCase::IgnoreCase)
			&& !Url.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase))
		{
			Url = TEXT("http://") + Url;
		}
		while (Url.RemoveFromEnd(TEXT("/")))
		{
		}
		if (Url.EndsWith(TEXT("/api/v1"), ESearchCase::IgnoreCase))
		{
			return Url;
		}
		// Host only (no path) → append /api/v1. Custom prefixes are left as-is.
		FString Scheme, Rest;
		if (Url.Split(TEXT("://"), &Scheme, &Rest))
		{
			int32 SlashIdx = INDEX_NONE;
			if (!Rest.FindChar(TEXT('/'), SlashIdx))
			{
				Url += TEXT("/api/v1");
			}
			else
			{
				UE_LOG(LogTemp, Warning,
					TEXT("FastGame: ApiBaseUrl '%s' has a path but does not end with /api/v1. "
						 "Login expects …/api/v1/base/login/access-token"),
					*Url);
			}
		}
		return Url;
	}
}

FFastGameHttp::FFastGameHttp(FString InApiBaseUrl)
	: ApiBaseUrl(FastGameHttpUtil::NormalizeApiBaseUrl(MoveTemp(InApiBaseUrl)))
{
	UE_LOG(LogTemp, Log, TEXT("FastGame: ApiBaseUrl=%s"), *ApiBaseUrl);
}

void FFastGameHttp::Request(
	const FString& Verb,
	const FString& Path,
	const FString& Body,
	const FString& ContentType,
	FOnHttpComplete OnComplete)
{
	FString Url = Path.StartsWith(TEXT("http")) ? Path : ApiBaseUrl + (Path.StartsWith(TEXT("/")) ? Path : TEXT("/") + Path);
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(Url);
	Req->SetVerb(Verb);
	if (!ContentType.IsEmpty())
	{
		Req->SetHeader(TEXT("Content-Type"), ContentType);
	}
	if (!AccessToken.IsEmpty())
	{
		Req->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + AccessToken);
	}
	if (!Body.IsEmpty())
	{
		Req->SetContentAsString(Body);
	}
	Req->OnProcessRequestComplete().BindLambda(
		[OnComplete](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
		{
			if (!OnComplete) return;
			if (!bConnected || !Response.IsValid())
			{
				OnComplete(false, 0, TEXT(""), TEXT("Network error"));
				return;
			}
			const int32 Code = Response->GetResponseCode();
			const FString RespBody = Response->GetContentAsString();
			if (Code < 200 || Code >= 300)
			{
				OnComplete(false, Code, RespBody, FString::Printf(TEXT("%d: %s"), Code, *RespBody));
				return;
			}
			OnComplete(true, Code, RespBody, TEXT(""));
		});
	Req->ProcessRequest();
}

void FFastGameHttp::Get(const FString& Path, FOnHttpComplete OnComplete)
{
	Request(TEXT("GET"), Path, TEXT(""), TEXT(""), OnComplete);
}

void FFastGameHttp::PostJson(const FString& Path, const FString& JsonBody, FOnHttpComplete OnComplete)
{
	Request(TEXT("POST"), Path, JsonBody, TEXT("application/json"), OnComplete);
}

void FFastGameHttp::PutJson(const FString& Path, const FString& JsonBody, FOnHttpComplete OnComplete)
{
	Request(TEXT("PUT"), Path, JsonBody, TEXT("application/json"), OnComplete);
}

void FFastGameHttp::PatchJson(const FString& Path, const FString& JsonBody, FOnHttpComplete OnComplete)
{
	Request(TEXT("PATCH"), Path, JsonBody, TEXT("application/json"), OnComplete);
}

void FFastGameHttp::PostForm(const FString& Path, const TMap<FString, FString>& Fields, FOnHttpComplete OnComplete)
{
	TArray<FString> Parts;
	for (const auto& Kv : Fields)
	{
		Parts.Add(FGenericPlatformHttp::UrlEncode(Kv.Key) + TEXT("=") + FGenericPlatformHttp::UrlEncode(Kv.Value));
	}
	Request(TEXT("POST"), Path, FString::Join(Parts, TEXT("&")), TEXT("application/x-www-form-urlencoded"), OnComplete);
}

FString FFastGameHttp::ExtractApiMessage(int32 StatusCode, const FString& Body, const FString& Fallback)
{
	if (!Body.IsEmpty())
	{
		if (const TSharedPtr<FJsonObject> Obj = FastGameHttpJsonUtil::ParseObject(Body))
		{
			FString DetailStr;
			if (Obj->TryGetStringField(TEXT("detail"), DetailStr) && !DetailStr.IsEmpty())
			{
				return DetailStr;
			}
			const TArray<TSharedPtr<FJsonValue>>* DetailArr = nullptr;
			if (Obj->TryGetArrayField(TEXT("detail"), DetailArr) && DetailArr && DetailArr->Num() > 0)
			{
				TArray<FString> Parts;
				for (const TSharedPtr<FJsonValue>& V : *DetailArr)
				{
					if (!V.IsValid()) continue;
					if (V->Type == EJson::String)
					{
						Parts.Add(V->AsString());
					}
					else if (V->Type == EJson::Object)
					{
						FString Msg;
						if (V->AsObject()->TryGetStringField(TEXT("msg"), Msg) && !Msg.IsEmpty())
						{
							Parts.Add(Msg);
						}
					}
				}
				if (Parts.Num() > 0)
				{
					return FString::Join(Parts, TEXT("; "));
				}
			}
			FString Message;
			if (Obj->TryGetStringField(TEXT("message"), Message) && !Message.IsEmpty())
			{
				return Message;
			}
		}
		return Body;
	}
	if (!Fallback.IsEmpty())
	{
		return Fallback;
	}
	if (StatusCode > 0)
	{
		return FString::Printf(TEXT("HTTP %d"), StatusCode);
	}
	return TEXT("Request failed");
}

FString FFastGameHttp::ExtractShopUnlockMessage(const FString& Body, const FString& Fallback)
{
	if (Body.IsEmpty())
	{
		return Fallback;
	}
	if (const TSharedPtr<FJsonObject> Obj = FastGameHttpJsonUtil::ParseObject(Body))
	{
		FString Gateway;
		if (Obj->TryGetStringField(TEXT("gateway_message"), Gateway) && !Gateway.IsEmpty())
		{
			return Gateway;
		}
		FString Message;
		if (Obj->TryGetStringField(TEXT("message"), Message) && !Message.IsEmpty())
		{
			return Message;
		}
	}
	return ExtractApiMessage(0, Body, Fallback);
}

void FFastGameHttp::ParseStatusFromError(bool bOk, const FString& Err, int32& OutCode, FString& OutMessage)
{
	if (bOk)
	{
		OutCode = 200;
		OutMessage.Reset();
		return;
	}

	int32 ColonIdx = INDEX_NONE;
	if (Err.FindChar(TEXT(':'), ColonIdx) && ColonIdx > 0)
	{
		const FString CodeStr = Err.Left(ColonIdx).TrimStartAndEnd();
		if (CodeStr.IsNumeric())
		{
			OutCode = FCString::Atoi(*CodeStr);
			const FString Body = Err.Mid(ColonIdx + 1).TrimStart();
			OutMessage = ExtractApiMessage(OutCode, Body, Err);
			return;
		}
	}

	OutCode = 0;
	OutMessage = Err;
}

FString FFastGameHttp::AppendI18nQuery(const FString& Path, const FString& Lang, bool bExpandI18n)
{
	if (Lang.IsEmpty() && !bExpandI18n)
	{
		return Path;
	}
	FString Out = Path;
	bool bNeedAmp = Path.Contains(TEXT("?"));
	auto Add = [&](const FString& Key, const FString& Value)
	{
		Out += bNeedAmp ? TEXT("&") : TEXT("?");
		bNeedAmp = true;
		Out += Key;
		Out += TEXT("=");
		Out += FGenericPlatformHttp::UrlEncode(Value);
	};
	if (!Lang.IsEmpty())
	{
		Add(TEXT("lang"), Lang);
	}
	if (bExpandI18n)
	{
		Add(TEXT("expand_i18n"), TEXT("true"));
	}
	return Out;
}
