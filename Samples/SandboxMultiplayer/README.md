# SandboxMultiplayer sample (Unreal)

**Two packages:** Fast Game (FastAPI) + [colyseus-unreal](https://github.com/soshyant-joshaghani/colyseus-unreal) (sibling fork).

1. Enable **FastGame** plugin.
2. Run `Scripts\fetch-colyseus.bat` — installs our fork into `Plugins/colyseus-unreal/` (gitignored). Use `-Update` to pull fork `main`, then pin the printed SHA in `Scripts/colyseus.lock.json`.
3. Enable plugin **Colyseus** in Edit → Plugins.
4. `PrepareSession` + `GetGameServer` via Fast Game.
5. `JoinOrCreate` / `Send` / `Leave` via Colyseus — sketch in `FastGameColyseusWire.h`.

Join options: `{ gameId, modeId, mapId }`. Sandbox messages: `move`, `score`, `finish`.
