#pragma once

#include "FastGameClient.h"

/**
 * Copy into your game module and call from an Actor/GameInstance.
 * No Colyseus required.
 */
inline void FastGameRunApiOnlyDemo(
	const FString& ApiBaseUrl,
	const FString& Identity,
	const FString& Password,
	const FString& GameId,
	const FString& ModeId,
	const FString& MapId)
{
	FFastGameConfig Config;
	Config.ApiBaseUrl = ApiBaseUrl;
	TSharedRef<FFastGameClient> Client = MakeShared<FFastGameClient>(Config);

	Client->Auth->Login(Identity, Password, [Client, GameId, ModeId, MapId](bool bOk, int32 StatusCode, FString /*AccessToken*/, FString Message)
	{
		if (!bOk)
		{
			UE_LOG(LogTemp, Error, TEXT("FastGame login [%d]: %s"), StatusCode, *Message);
			return;
		}
		Client->Content->PrepareSession(GameId, ModeId, MapId,
			[Client, GameId](bool bOk2, FFastGamePreparedSession Session, FString Err2)
			{
				if (!bOk2)
				{
					UE_LOG(LogTemp, Error, TEXT("PrepareSession: %s"), *Err2);
					return;
				}
				UE_LOG(LogTemp, Log, TEXT("PrepareSession ok room=%s"), *Session.ColyseusRoom);
				Client->Shop->GetCatalog(GameId, [](bool bOk3, TArray<FFastGameShopLine> Lines, FString Err3)
				{
					if (!bOk3)
					{
						UE_LOG(LogTemp, Error, TEXT("Shop: %s"), *Err3);
						return;
					}
					UE_LOG(LogTemp, Log, TEXT("Shop lines=%d"), Lines.Num());
				});
			});
	});
}
