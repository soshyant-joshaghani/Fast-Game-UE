#pragma once

#include "CoreMinimal.h"
#include "FastGame.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

class FASTGAME_API FFastGameHttp
{
public:
	explicit FFastGameHttp(FString InApiBaseUrl);

	void SetAccessToken(const FString& Token) { AccessToken = Token; }
	const FString& GetAccessToken() const { return AccessToken; }
	bool IsLoggedIn() const { return !AccessToken.IsEmpty(); }

	using FOnHttpComplete = TFunction<void(bool /*bOk*/, int32 /*Code*/, FString /*Body*/, FString /*Error*/)>;

	void Request(
		const FString& Verb,
		const FString& Path,
		const FString& Body,
		const FString& ContentType,
		FOnHttpComplete OnComplete);

	void Get(const FString& Path, FOnHttpComplete OnComplete);
	void PostJson(const FString& Path, const FString& JsonBody, FOnHttpComplete OnComplete);
	void PutJson(const FString& Path, const FString& JsonBody, FOnHttpComplete OnComplete);
	void PatchJson(const FString& Path, const FString& JsonBody, FOnHttpComplete OnComplete);
	void PostForm(const FString& Path, const TMap<FString, FString>& Fields, FOnHttpComplete OnComplete);

	/** Prefer FastAPI `detail` / `message`; fall back to raw body or Fallback. */
	static FString ExtractApiMessage(int32 StatusCode, const FString& Body, const FString& Fallback);

	/** Shop unlock/restore/complete JSON: gateway_message, message, then detail. */
	static FString ExtractShopUnlockMessage(const FString& Body, const FString& Fallback = TEXT(""));

	/** On success: Code=200, Message empty. On failure: parse "code: body" Err into StatusCode + Message. */
	static void ParseStatusFromError(bool bOk, const FString& Err, int32& OutCode, FString& OutMessage);

	/** Append foxg-back style ``lang`` / ``expand_i18n`` query params (no-op when both unset). */
	static FString AppendI18nQuery(const FString& Path, const FString& Lang, bool bExpandI18n = false);

	FString ApiBaseUrl;

private:
	FString AccessToken;
};
