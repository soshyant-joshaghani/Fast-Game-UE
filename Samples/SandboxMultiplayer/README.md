# SandboxMultiplayer sample (Unreal)

**Two packages:** Fast Game (FastAPI) + [colyseus-unreal](https://github.com/charisma-ai/colyseus-unreal) (sibling).

1. Enable **FastGame** plugin.
2. Run `Scripts\fetch-colyseus.bat` — installs [colyseus-unreal](https://github.com/charisma-ai/colyseus-unreal) into `Plugins/colyseus-unreal/` (gitignored). Use `-Update` to sync upstream.
3. Enable plugin **Colyseus** in Edit → Plugins.
4. `PrepareSession` + `GetGameServer` via Fast Game.
5. `JoinOrCreate` / `Send` / `Leave` via Colyseus — sketch in `FastGameColyseusWire.h`.

Join options: `{ gameId, modeId, mapId }`. Sandbox messages: `move`, `score`, `finish`.
