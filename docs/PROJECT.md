# Fast Game UE — project guide

Guide for **this** Unreal project: what exists today, how it relates to Fast Game, and production rules to follow when game content lands here.

For plugin API usage see [SDK.md](../SDK.md). For HTTP contracts see [CONTRACT.md](../CONTRACT.md).

---

## What this repo is today

| Status | Item |
|--------|------|
| **Exists** | UE **5.6.1** project (`FastGameUE.uproject`) |
| **Exists** | [`Plugins/FastGame`](../Plugins/FastGame) — FastAPI client (Blueprint + C++) |
| **Exists** | [`Plugins/FastGameStore`](../Plugins/FastGameStore) — Android IAP (Myket / Cafe Bazaar / Google Play) |
| **Exists** | [`Samples/`](../Samples/) — API-only + Colyseus join sketches |
| **Exists** | [`tests/`](../tests/) + [`run_tests.py`](../run_tests.py) — SDK contract checks |
| **Exists** | Thin game module [`Source/FastGameUE/`](../Source/FastGameUE/) |
| **Not yet** | `Content/` game assets, maps, gameplay Blueprints |
| **Not yet** | Platform profile system, asset registry, CLI build wrapper |
| **Not yet** | Android/Windows packaging scripts in-repo |

This is an **SDK development shell** that can grow into a full Android + PC game **in the same project**. It is not a blank game and not a finished game.

---

## Fast Game stack (do not reimplement)

The temp platform spec assumed a greenfield game. This project already has backend integration:

| Concern | Use |
|---------|-----|
| Auth, catalog, content, shop, ads | **`FastGame`** plugin → [`SDK.md`](../SDK.md) |
| Android store IAP | **`FastGameStore`** plugin — one store flavor **per APK**; designers call **Unlock Sku** only |
| Backend | [`fast-game`](../fast-game/Readme.md) dev stack (`http://api.localhost/api/v1`) |
| Multiplayer rooms | **Sibling** [colyseus-unreal](https://github.com/charisma-ai/colyseus-unreal) — `Scripts/fetch-colyseus.bat` → `Plugins/colyseus-unreal/` |
| API contract | [`CONTRACT.md`](../CONTRACT.md) |

**Initialize Game** (once): `GameCode` + `StorePlatform` (install check on device).  
**Initialize Client** (reconnect): full API URL. Auth OTP / shop empty pins use Initialize Game — not per-widget args.

Do **not** add a second shop layer, custom HTTP client, or Colyseus wrapper inside game code unless you have a specific reason documented in `CONTRACT.md`.

---

## Evaluated production vision

The old `ue project configs.txt` draft described a **mobile-first, single-project** pipeline (Android + Windows, shared gameplay, platform-specific presentation). Most of that still applies **when you add game content**. Below: what to keep, what to defer, and what is already covered.

### Keep (game production rules)

1. **One Unreal project** for Android and PC — no separate mobile project or end-of-project “port”.
2. **Shared gameplay** — one `BP_Player`, one ruleset; platform differences in presentation/performance, not duplicated game logic.
3. **Central platform/quality abstraction** — e.g. `GetAssetVariant()`, texture/VFX/foliage tiers; avoid `if (Android)` scattered in Blueprints.
4. **Asset variants only when worth it** — not every mesh needs `_Mobile` / `_PC`; each variant has its **own LOD chain**.
5. **Mobile-first rendering** — do not depend on Nanite, Lumen, VSM, or heavy RT for core gameplay; PC can add quality on top.
6. **Materials & textures** — know Android cost per gameplay material; cap texture sizes on mobile (often 1K–2K vs 2K–4K PC).
7. **Enhanced Input** — platform-agnostic actions (`IA_Move`, `IA_Interact`, …) with separate mapping contexts for touch vs KBM.
8. **Budgets before scale** — define FPS, memory, draw calls, and package size per **target Android device** early; profile on real hardware.
9. **Definition of done** — a feature is not done on PC alone; it must work and perform on the target Android device.
10. **Continuous Android builds** — ship Android Development builds throughout milestones, not only at the end.

### Defer (not in repo yet — add when game work starts)

| Draft idea | Recommendation |
|------------|----------------|
| `PlatformService` C++ module | Add under `Source/FastGameUE` or `Plugins/` when first maps exist |
| `DA_*` logical asset registry | Introduce when environment asset count justifies it |
| `game build android` CLI wrapper | Wrap `RunUAT BuildCookRun` in `Scripts/` when packaging is routine |
| `Build/Android.json` profiles | Add with first CI job |
| `game validate assets` | Custom editor utility or commandlet — after Content/ grows |
| World Partition | Only if open-world size requires it; not default for small/medium maps |
| HLOD / level streaming | Per-map decision; separate from per-asset platform variants |

### Already solved (ignore duplicate advice from generic AI spec)

- Store billing / restore / RSA → **FastGameStore** + **Unlock Sku**
- FastAPI paths / ENTER auth / shop unlock → **FastGame** subsystem
- SDK tests → `py -3 run_tests.py`

---

## Current layout vs planned

```text
fast-game-ue/                          Today          When game lands
├── FastGameUE.uproject              ✓              ✓
├── Config/                            minimal        platform DeviceProfiles, input, packaging
├── Source/FastGameUE/                 shell module   + PlatformService, game systems
├── Plugins/
│   ├── FastGame/                      ✓              ✓ (upstream SDK)
│   └── FastGameStore/                 ✓              ✓ (one flavor per store APK)
├── Samples/                           ✓              keep as reference
├── tests/                             ✓              ✓ (+ optional game validation)
├── Content/                           —              see below
├── Scripts/                           —              RunUAT wrappers, CI entrypoints
└── docs/PROJECT.md                    ✓              this file
```

Suggested **Content/** layout when you start (aligned with the evaluated spec, plus Fast Game UI):

```text
Content/
├── Core/
│   ├── Gameplay/
│   ├── Characters/
│   ├── Systems/
│   ├── UI/              # wire to Fast Game Auth/Shop widgets or custom + subsystem
│   ├── Input/           # Enhanced Input actions + IMC_PC / IMC_Mobile
│   └── Audio/
├── Environment/
├── Platform/            # optional Mobile/ PC mesh & material variants
├── Maps/
├── Data/                # DataAssets / tables for logical asset IDs
├── VFX/
└── Developer/           # dev-only maps & tests
```

Naming (when variants exist): `SM_Tree_01_Mobile`, `SM_Tree_01_PC`; logical id `Tree_01` in a DataAsset.

---

## Android + PC targets

**Platforms:** Android + Windows PC from one project.

**Android store builds:**

- Set **Initialize Game → Store Platform** to match the APK (`myket` / `caffebazar` / `googleplay`).
- Copy **one** Java/Kotlin flavor into the store plugin tree before packaging — see [FastGameStore README](../Plugins/FastGameStore/README.md).
- Fast Game login (e.g. phone) need **not** match the store wallet email; ownership is Fast Game user + receipt.

**Rendering (mobile-first defaults to set in `Config/` when content exists):**

- Prefer baked/static lighting where possible; limit dynamic lights on Android.
- No Nanite dependency on Android; PC may use Nanite on high-detail variants only.
- Niagara: scalability tiers (particle count, lifetime, lights), not duplicate gameplay systems.

**Input:** touch-safe UI; no hard-coded mouse-only flows in shared gameplay Blueprints.

---

## Build matrix (target state)

|  | Development | Shipping |
|--|-------------|----------|
| **Android** | Profile + iterate | Store / sideload validation |
| **Windows** | Daily PC test | PC release |

Use Unreal’s pipeline (`RunUAT`, `BuildCookRun`) — do not replace it. A thin `Scripts/build.bat` (or similar) can wrap the same args for local and CI.

**CI-ready goal:** same command locally and in GitLab/GitHub Actions, e.g. `Scripts\build-android-shipping.bat`.

Not implemented yet; Milestone 0 below.

---

## Milestones (adapted to this repo)

### Milestone 0 — Foundation (partial)

- [x] UE 5.6 project opens; **FastGame** + **FastGameStore** compile
- [x] SDK samples + `run_tests.py`
- [x] Backend contract documented
- [ ] `Content/` game module structure
- [ ] First custom map (replace engine template default in `Config/DefaultEngine.ini`)
- [ ] Android Development package from CLI
- [ ] Windows Development package from CLI
- [ ] Documented target Android device(s)

### Milestone 1 — First playable

- [ ] Player + Enhanced Input (PC + Android)
- [ ] **Initialize Game / Initialize Client** wired in game instance
- [ ] Login + catalog smoke test against dev `fast-game`
- [ ] Android + PC Development builds of the same map

### Milestone 2 — Vertical slice

- [ ] Full loop: auth → content/session → gameplay → shop (if applicable)
- [ ] Platform profiles + first asset variants where needed
- [ ] Profiling pass on target Android hardware
- [ ] Optional: Colyseus sibling plugin pinned for multiplayer slice

### Milestone 3 — Production / release

- [ ] Shipping builds both platforms via same script path as dev
- [ ] Store APK per flavor with matching **StorePlatform**
- [ ] Performance, memory, loading, and package size within budgets

---

## Workflow

For each feature:

```text
Design → Implement → PC test → Android test (target device) → Profile → Commit
```

Hard rule (from evaluated spec):

> If it cannot be built and tested on Android during development, it is not production-ready.

Pair with Fast Game rule from [CONTRACT.md](../CONTRACT.md): client-facing API changes must pass **`fast-game` frontend** + **both SDK projects** tests.

---

## Architecture (target)

```text
ONE UNREAL PROJECT (fast-game-ue)
        |
   GAMEPLAY (shared)          PLATFORM (presentation)
        |                            |
   FastGameUE module          Mobile profile / PC profile
   FastGame subsystem               |
   Colyseus (optional)        Asset variants, LOD, materials
        |                            |
        +-------- Android APK / Windows exe --------+
                      (RunUAT / CI)
```

PC is a **higher-quality view of the same game**, not a separate source that gets downgraded at the end.

---

## Related docs

| Doc | Purpose |
|-----|---------|
| [README.md](../README.md) | Quick open + test |
| [SDK.md](../SDK.md) | Fast Game plugin Blueprint/C++ |
| [CONTRACT.md](../CONTRACT.md) | HTTP + SDK contract |
| [Plugins/FastGameStore/README.md](../Plugins/FastGameStore/README.md) | Android IAP flavors |
