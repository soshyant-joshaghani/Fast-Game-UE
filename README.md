# Fast Game UE

Standalone **Unreal Engine 5.6.1** project for the official Fast Game client SDK.

| Item | Path |
|------|------|
| Project | `FastGameUE.uproject` |
| Fast Game plugin | `Plugins/FastGame` |
| Android IAP plugin | `Plugins/FastGameStore` |
| Samples | `Samples/` |
| SDK docs | [SDK.md](SDK.md) |
| Project guide | [docs/PROJECT.md](docs/PROJECT.md) |
| Contract | [CONTRACT.md](CONTRACT.md) |
| Backend kit | [`../fast-game/`](../fast-game/Readme.md) |

## Open

1. Unreal Engine **5.6.1** → Open `FastGameUE.uproject`
2. Allow plugin rebuild on first open
3. Point **Initialize Client** at a running Fast Game API, e.g. `http://api.localhost/api/v1`

## Tests

```bat
py -3 run_tests.py
```

Contract + Android store compile checks under `tests/`.

## Multiplayer (Colyseus sibling)

Fast Game does **not** include Colyseus. Fetch the sibling plugin, then enable it in the editor:

```bat
Scripts\fetch-colyseus.bat
Scripts\fetch-colyseus.bat -Update
```

| Item | Detail |
|------|--------|
| Sibling (ours) | [soshyant-joshaghani/colyseus-unreal](https://github.com/soshyant-joshaghani/colyseus-unreal) — fork of [charisma-ai-colyseus-ue](https://github.com/charisma-ai/colyseus-unreal) |
| Installed to | `Plugins/colyseus-unreal/` (gitignored) |
| Pin / update | `Scripts/colyseus.lock.json` pins a commit SHA · `-Update` pulls fork `main` and prints the SHA to pin |

After fetch: enable plugin **Colyseus** in Edit → Plugins. See `Samples/SandboxMultiplayer/`.
