/**
 * Sibling Colyseus (colyseus-unreal) — not part of Fast Game.
 *
 * Flow:
 *   1. FastGame: Login + PrepareSession → ColyseusRoom
 *   2. FastGame: Catalog->GetGameServer → endpoint URL
 *   3. Colyseus sibling: JoinOrCreate(room, { gameId, modeId, mapId })
 *
 * Example (adjust types to your colyseus-unreal version):
 *
 *   #include "Client.h"
 *   #include "Room.h"
 *   #include "FastGameClient.h"
 *
 *   Client->Content->PrepareSession(GameId, ModeId, MapId,
 *     [Client](bool bOk, FFastGamePreparedSession Session, FString Err)
 *     {
 *       Client->Catalog->GetGameServer([Session](bool bOk2, FString Url, FString Err2)
 *       {
 *         TSharedPtr<Colyseus::Client> Coly = MakeShared<Colyseus::Client>(Url);
 *         Coly->JoinOrCreate<void>(Session.ColyseusRoom,
 *           {{"gameId", Session.GameId}, {"modeId", Session.ModeId}, {"mapId", Session.MapId}},
 *           [](TSharedPtr<Colyseus::MatchMakeError> Error, TSharedPtr<Colyseus::Room<void>> Room)
 *           {
 *             // Room->Send("move", ...); etc.
 *           });
 *       });
 *     });
 */

#pragma once

#include "FastGameClient.h"

namespace FastGameSamples
{
	inline void LogSiblingColyseusHint()
	{
		UE_LOG(LogTemp, Log,
			TEXT("FastGame: use sibling colyseus-unreal for JoinOrCreate — see Samples/SandboxMultiplayer/README.md"));
	}
}
