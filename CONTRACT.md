# Fast Game SDK contract

Official client surface for Unity and Unreal. Aligns with [fast-game/docs/studio-integration.md](../fast-game/docs/studio-integration.md) and the web panel clients under `fast-game/frontend/`. Studio buy/restore graphs (Unity + Unreal, `little_guardians` / `full_game`, Myket ↔ Cafe Bazaar): [fast-game/docs/sdk-shop-flows.md](../fast-game/docs/sdk-shop-flows.md).

## Stack

| Layer | Role |
|-------|------|
| **Fast Game SDK** | Named FastAPI APIs: Auth, Catalog, Content, Shop, Assets, Ads |
| **Colyseus SDK** (sibling package) | Multiplayer rooms — install separately; use catalog `colyseus_room` + `Catalog.GetGameServer` |

Fast Game does **not** wrap Colyseus join/send/leave. Studios call the Colyseus SDK directly.

## Named modules (FastAPI only)

| Module | Entry | Primary functions |
|--------|-------|-------------------|
| Auth | `client.Auth` | `Enter` (**stores** identity; no `game_code` pin) → UE Blueprint pins **Enter Password** / **Verify** / **Signup** / **Failed** (seeded `password_required` fires **Signup**; LastEnterRoute stays CompleteAccount). Internal routes: Login / CompleteAccount / VerifyId / Register via catalog `auth_requirements` for **client `GameCode`**. `Login` / `Register` (Signup; auto `/complete` when LastEnterRoute is CompleteAccount) / Recovery / Signup OTP with optional identity (empty → ENTER store); `Send Auth Code` (Verify → signup OTP, Enter Password → recovery OTP); `Verify Auth Code` pins **Signup** \| **Assign New Password** \| **Failed**; `Assign New Password` (recovery confirm); `UpdateFullName` (`PATCH /me`); UE latent auth nodes: **Success** \| **Failed** exec pins (no redundant `bSuccess`); Check Authentication: **Authenticated** \| **Not Authenticated** \| **Failed**. `ClearEnteredIdentity` / `GetEnteredIdentity` / `HasEnteredIdentity`; `GetMe`, `Logout`, `ClearLocalCache` (also clears ENTER store), `IsLoggedIn`, `AccessToken`, `CurrentUser`. Auth OTP/recovery inject backend `game_code` from client config (`Initialize Game` / `SetGameCode`), not method args. |
| Catalog | `client.Catalog` | `ListGames` / `GetGame` (`lang`, `expandI18n`), `GetAuthRequirements` (public), `GetGameServer` |

| Content | `client.Content` | `ListCharacters`, `GetMapRuntime`, `ResolveSpawn`, `PrepareSession` (same `lang` / `expandI18n`), loadout / claim helpers |
| Shop | `client.Shop` | `GetCatalog` (`lang`, `expandI18n`; empty `GameId` → Initialize Game `GameCode`), `ClaimFree`, `RedeemCode`, **Unlock Sku** / **Complete Unlock** (`POST …/shop/unlock/begin|complete`), **Shop Progress** / Unlock / Complete Unlock (exec pins: Purchase Successful / Pending / Failed / Cancelled / Store Missing), `GetShopSkuAccess` (UE pins: Owned / Available / Locked / Failed), Claim/Redeem/Catalog Success \| Failed. Empty shop `GameCode` / `Provider` → Initialize Game (same as Enter empty Identity). Unlock Sku has no Provider pin — always Initialize Game `StorePlatform`. |
| Store | `FastGameStore` (internal OS) | Android Myket / Cafe Bazaar / Google Play. One flavor per APK, **inside** Unity `Plugins/Android/FastGameStore` and Unreal `Plugins/FastGameStore`. **Automatically follows** Initialize Game (install check + public key). Designers do **not** call FastGameStore — use Fast Game **Unlock Sku**. iOS StoreKit later (`unity/Plugins/iOS`, `unreal/Plugins/FastGameStore/iOS`). |
| Assets | `client.Assets` | Pack `url` + `hash` helpers from catalog detail |
| Ads | `client.Ads` | `GetAdvertisement` (null / `bHasAd=false` on HTTP 204), `TrackEvent` (`AdvertisementDisplayed` / `Clicked` / `Closed`) |

## HTTP paths (client)

Base: `{ApiBaseUrl}` default `http://api.localhost/api/v1`

| Area | Method | Path |
|------|--------|------|
| Enter | POST | `/base/login/enter` (JSON: `identity?` or `email?` / `phone?`) — **ENTER contract** (studio auth routing, no secrets). Response: `exists`, `password_required`, `channel` (`email`\|`phone`), `email`, `phone` (exactly one contact filled, normalized). Always **200** for valid contact (including unknown users). No `game_code`. Client internal routes: `exists && !password_required` → **Login**; `exists && password_required` → **CompleteAccount** (no OTP); `!exists` + verify ON → **VerifyId**; `!exists` + verify OFF → **Register**. UE Blueprint Enter pins fold CompleteAccount into **Signup** (Register dispatches `/complete`). Forgot is **not** an Enter pin — from Enter Password call **Send Auth Code** (recovery). SDKs **store** the normalized identity on success. |
| Auth requirements | GET | `/apps/games/catalog/{game_code}/auth-requirements` — **public** `{ verify_phone, verify_email }` (provider ready + flag). Used after Enter for new-user **VerifyId** vs **Register**. |
| Store RSA | GET | `/apps/games/catalog/{game_code}/store-verify-key?provider=myket\|caffebazar` — login required. FG1-wrapped **RSA public key** for on-device Myket/Bazaar verify (`configured`, `encoding`, `rsa_verify_key`). Never `api_secret` / JWT / Myket `access_token`. SDK fetches after login; Blueprint **Set Store Public Key** is an optional override. |
| Auth | POST | `/base/login/access-token` (form: `username` = email **or** phone, `password`) — SDKs use `Channel` Auto/Email/Phone. Empty Identity on Login → ENTER-stored identity. Backend `find_user_by_identity`. **403** `password_required` if account has no password (prefer Enter → Complete Account) |
| Me | GET | `/base/login/me` (Bearer) → user public profile (no password) |
| Me update | PATCH | `/base/login/me` (Bearer, JSON: `{ "full_name": "…" }` only) — display name; not the login id |
| Complete account | POST | `/base/login/complete` (JSON: `email?`, `phone?`, `password`, `full_name?`) — passwordless existing user first-set (**no OTP**). **400** if user not found or password already set. Marks contact verified; SDKs then auto-login |
| Signup | POST | `/base/users/signup` (JSON: `email?`, `phone?`, `password`, `full_name?`, optional `game_code` / `code`) — **new** users only (**400** if already exists). With `game_code` + channel verify enabled, requires prior signup OTP (`signup_verified` or one-shot `code`). SDKs: empty Email+Phone → ENTER store; require password confirm locally (not sent), then auto-login |
| Signup OTP request | POST | `/base/signup/request` (JSON: `game_code`, `email?`, `phone?`) — send OTP for **new** identity when verify is on; fails if user exists |
| Signup OTP verify | POST | `/base/signup/verify` (JSON: `game_code`, `code`, `email?`, `phone?`) — OTP only; then show Signup |
| Recovery request | POST | `/base/recovery/request` (JSON: `game_code`, `email?`, `phone?` — at least one) — send OTP; SDKs: empty Identity → ENTER store |
| Recovery verify | POST | `/base/recovery/verify` (JSON: `game_code`, `code`, `email?`, `phone?`) — OTP only; empty Identity → ENTER store |
| Recovery confirm | POST | `/base/recovery/confirm` (JSON: `game_code`, `new_password`, `code?`, `email?`, `phone?`) — after **Forgot** OTP verify, omit `code`; **no** `full_name`; empty Identity → ENTER store; SDKs verify password confirm locally |
| Set-password (alias) | POST | `/base/password/set/request\|verify\|confirm` — **deprecated** thin aliases of `/base/recovery/*` (same OTP purposes); prefer recovery |
| Phone verify request | POST | `/base/login/phone/verification` (Bearer, JSON: `game_code`, `phone?`) — only when game `verify_phone` + SMS provider ready (post-login contact verify) |
| Phone verify confirm | POST | `/base/login/phone/verification/confirm` (Bearer, JSON: `game_code`, `code`, `phone?`) |
| Catalog | GET | `/apps/games/catalog/?available_only=` |
| Catalog detail | GET | `/apps/games/catalog/{game_id}` |
| Characters | GET | `/apps/games/content/{game}/characters?role=player\|npc` |
| Cosmetics | GET | `/apps/games/content/{game}/characters/{character_id}/cosmetics` |
| Abilities | GET | `/apps/games/content/{game}/characters/{character_id}/abilities` |
| Map runtime | GET | `/apps/games/content/{game}/maps/{map_id}/runtime` |
| Character placements | GET | `/apps/games/content/{game}/maps/{map_id}/character-placements` |
| Event | GET | `/apps/games/content/{game}/events/{event_id}` |
| Achievement | GET | `/apps/games/content/{game}/achievements/{achievement_id}` |
| Avatar | GET | `/apps/games/content/{game}/avatars/{avatar_id}` |
| Title | GET | `/apps/games/content/{game}/titles/{title_id}` |
| Event claim | POST | `/apps/games/content/{game}/events/{event_id}/claim` (alias `/trigger`) |
| Spawn | POST | `/apps/games/content/{game}/players/me/spawn` |
| Loadout | GET/PUT | `/apps/games/content/{game}/players/me/loadout` |
| Activate avatar | POST | `/apps/games/content/{game}/players/me/avatars/{avatar_id}/activate` |
| Activate title | POST | `/apps/games/content/{game}/players/me/titles/{title_id}/activate` |
| Pickup claim | POST | `/apps/games/content/{game}/players/me/pickup-claim` |
| Shop catalog | GET | `/apps/games/shop/catalog` |
| Claim free | POST | `/apps/games/shop/claim-free` |
| Unlock begin | POST | `/apps/games/shop/unlock/begin` — `{ game_code, sku_kind, sku_id, provider, callback_url?, discount_code? }` → already `owned` or `pending` + `mode` (`zarinpal`\|`store`\|`steam`) + `authority` / `payment_token` / `payment_url?` / `store_product_id?` / `order_id?` |
| Unlock complete | POST | `/apps/games/shop/unlock/complete` — `{ authority, payment_token, purchase_token? }` → `{ success, owned }` (provider from `payment_intent`; `purchase_token` required for store) |
| Unlock restore | POST | `/apps/games/shop/unlock/restore` — `{ game_code, sku_kind, sku_id, provider, purchase_token }` → Myket/Bazaar/Play verify + persist `content_ownership` `{ success, owned }` |
| Store lock | POST | `/apps/games/shop/store-lock` — `{ game_code, provider }` start freeze. The wallet is the **first verified** Myket/Bazaar/Play token (old IAP restore allowed until then). After that, other old tokens 403. Fresh checkouts (recent `purchaseTime`) still allowed. |
| Redeem discount code | POST | `/apps/games/shop/redeem-code` |
| Provider currencies | GET/POST/PATCH/DELETE | `/apps/games/payments/{game}/providers/{provider}/currencies` |
| Pay initiate | POST | `/base/payments/initiate` — panel/web still valid; **games use shop unlock begin/complete**. For `metadata.kind=game_shop`, **amount is resolved server-side** from sku + provider + currency (client amount ignored) |
| Pay verify | POST | `/base/payments/verify` (optional `payment_token`; else `payment_intent` for current user). Games prefer **unlock/complete**. |
| Billing initiate | POST | `/base/payments/billing/initiate` (myket / caffebazar / googleplay) — panel/debug; games use **unlock/begin** |
| Billing submit | POST | `/base/payments/billing/submit` — panel/debug; games use **unlock/complete** |
| Steam initiate | POST | `/base/payments/steam/initiate` — panel/debug; games use **unlock/begin** |
| Steam finalize | POST | `/base/payments/steam/finalize` — panel/debug; games use **unlock/complete** |
| Steam link (ticket) | POST | `/base/steam/link` (`ticket` + `game_code`) |
| Steam OpenID login | GET | `/base/steam/login?game_code=` |
| Steam status | GET | `/base/steam/status` |
| Steam OpenID login | GET | `/base/steam/login` |
| Achievement Steam resync | POST | `/apps/games/content/{game}/achievements/steam/resync` |
| Game server | GET | `/utils/game-server/` |
| Ads request | POST | `/apps/games/ads/request` (JSON: `game_id`, optional `slot` / `media_type` / `format` / `tags` / `locale` / `country` / `platform` / `engine` / `capabilities`) → provider-opaque ad or **204** no fill |
| Ads events | POST | `/apps/games/ads/events` (JSON: `event_type`, optional `ad_id` / `game_id` / `campaign_id` / `timestamp` / `extras`) |

## Colyseus sibling (not part of this SDK)

Use values from FastAPI, then call Colyseus:

- Room name: prefer **map** `colyseus_room` when the map enables online modes; fall back to catalog `colyseus_room` for legacy games
- Endpoint: `GET /utils/game-server/` → `{ url }`
- Join options: `{ gameId, modeId, mapId }`
- Sandbox messages: `move` `{x,y,z}`, `score` `{delta}`, `finish` `{status}`; listen `match_outcome`

## DTO notes

- Ads: request uses extensible `capabilities` (e.g. `mediaTypes`); response is provider-opaque (`id`, `campaign_id`, `media`, `click`, `tracking`, `meta`) — never includes provider key. Empty fill is HTTP **204**. Media types: `image` | `gif` | `video` | `lottie` | `rive` | `text`. Text ads put `title` / `body` / `background_url` / `background_color` in `meta` (and UE Blueprint pins). **`media.url` / `meta.background_url` are absolute public CDN URLs** from the game’s storage targets (platform MinIO and/or Arvan / Liara / custom) — clients load them directly; no relative path rewriting. Client request pins `slot` / `locale` / `country` / `platform` / `engine` / `tags` are matched against campaign `targeting_rules` (lists; omit or empty = any). Configure those lists in the panel campaign editor — do not remove the SDK pins. UE: `Get Image Ad` / `Get Video Ad` / `Get Rive Ad` / `Get Text Ad` / … return typed `FFastGameBPAdvertisement` + URL pins; `Track Ad Displayed|Clicked|Closed`. Events: `AdvertisementDisplayed` | `AdvertisementClicked` | `AdvertisementClosed` (server also records `AdvertisementRequested` on `/request`).
- Characters have `role`: `player` | `npc` | `both` (old standalone NPC library removed); `body_kind` `modular` | `skins` | `simple` for customization mode; optional stats (`{}` when unused)
- Catalog modes: taxonomy ids toggled as booleans in the editor (rows in `game_mode`)
- Catalog `payment_providers`: booleans for `zarinpal` | `steam` | `googleplay` | `myket` | `caffebazar` | `stripe` | `paypal`
- Catalog `payment_config`: per-game credentials (editor). Secrets redacted on client catalog (`*_configured` flags). Shape includes `zarinpal` (`merchant_id`, `sandbox`), `myket` / `caffebazar` / `googleplay` (tokens/secrets + `package_name`), `steam` (`app_id`, keys, `realm`, `return_url`), plus optional `stripe` / `paypal` fields. No provider secrets in process env. **Cafe Bazaar / Myket RSA** is a public verify key: set it in Editor; clients fetch `GET …/catalog/{game}/store-verify-key` (FG1 wrap). Never put Pishkhan JWT / `api_secret` in the APK or Unreal.
- Steam link requires `game_code` so the server loads that title’s Steam `payment_config`
- Shop price `store_skus`: `{ myket, caffebazar, googleplay, steam }` store product / item ids used for receipt verification. **Required** for myket / caffebazar / googleplay billing (no Fast Game list/sale prices for those providers).
- Achievements: optional `steam_api_name` (defaults to `achievement_id`) for Steam Web API sync on ownership grant
- Provider currencies: rows on `game_provider_currency` for **zarinpal** / **steam** (and similar) — not for myket / caffebazar / googleplay (price lives in the store console)
- Maps: `supported_modes[]`, `sort_order`, optional per-map `colyseus_room` for online modes
- Map runtime: `player_spawns`, `character_placements`, `pickups`, plus `character_placements_by_mode` / `pickups_by_mode`; each placement carries `mode_id` + optional `team`; optional PRS `transform`
- Shop prices nested shape: `{ provider: { currency: { list, sale } } }` (e.g. `{ "zarinpal": { "rial": { "list": 10000, "sale": 8000 } } }`); bare numbers still accepted and stored as `list=sale`. Checkout uses **sale**.
- Unlock flags on content entities (maps, characters, cosmetics, modular parts, abilities, pickups, collectibles, rewards, achievements, avatars, titles): independent booleans **`locked`** and **`purchasable`**
  - neither → free/default starter content (not gated by shop or event)
  - `purchasable` → appears in shop catalog (including zero-price)
  - `locked` → may be granted via event claim assignments
  - both → unlock via purchase **or** event
  - Achievements/avatars/titles also expose derived `source`: `default` | `purchase` | `event` | `both`
- Shop catalog includes purchasable SKUs (including zero-price) with `owned` from `ContentOwnership` (bundle grants set owned)
- Discount codes: Editor creates codes (`free_unlock` | `percent_off` | `amount_off`); optional assignment gate. `RedeemCode` grants free unlock. Checkout `discount_code` on ZarinPal/Steam initiate applies percent/amount off **sale** price (not store IAP).
- Bundles: `GET/POST …/content/{game}/bundles`, `GET/PATCH/DELETE …/bundles/{bundle_id}` with `list_price` / `sale_price` and `items[]` (`map` | `character` | `cosmetic` | `modular_part` | `ability` | `collectible` | `pickup` | `reward` | `achievement` | `avatar` | `title`). May include locked and/or purchasable entities; buying the bundle grants all items. Editor under `/panel/editor/{game}/shop/bundles`
- Checkout resolve (zarinpal): uses `prices.zarinpal.<currency>.sale` (default `rial` when present)
- Checkout resolve (steam): nested `prices.steam.<currency>.sale` plus `store_skus.steam` item id
- Checkout resolve (myket / caffebazar / googleplay): **no Fast Game prices** — map via `store_skus.<provider>` only; native store SDK charges the console price; **unlock/begin** returns `store_product_id`, then **unlock/complete** with `purchase_token`
- Client build flavor is Initialize Game `StorePlatform` (must be catalog-enabled)
- **Lifecycle:** **Initialize Game** (1x) sets `GameCode` + `StorePlatform` and runs OS store-app install check. **Initialize Client** (Nx) is network / reconnect only (`ApiBaseUrl`, token restore; no Enter wipe; no install check).
- **Android store IAP:** one APK per store. Designers call **Unlock Sku** only. FastGameStore follows Initialize Game (`FastGameStoreActivity` extras `openTheStorePage` + `storeProductId` + optional `storePublicKey` → JNI `OnStorePurchase`). Cafe Bazaar / Myket **RSA** is fetched from Editor after login (`store-verify-key`); leave Unreal **Set Store Public Key** empty unless you need a local override. Cafe Bazaar checks `com.farsitel.bazaar`; Myket `ir.mservices.market`; Play `com.android.vending`. Missing store → fail, never fake `owned`. Fast Game Enter identity (phone/email) need **not** match the OS store wallet — restore binds `purchase_token` to the Fast Game user (one token → one user). Unity: `fast-game-unity/Packages/com.fastgame.sdk/Plugins/Android/FastGameStore`. Unreal: `Plugins/FastGameStore`. iOS StoreKit later (`Plugins/iOS`, `Plugins/FastGameStore/iOS`).
- Steam / ZarinPal stay Fast Game HTTP (finalize / verify) and grant the same `ContentOwnership`. Native Steamworks overlay stays outside this plugin.
- Steam account link required before Steam checkout; OpenID for web panel, session ticket for native
- Events: `POST .../events/{event_id}/claim` grants ownership for assignment id lists: `achievement_ids`, `avatar_ids`, `title_ids`, `map_ids`, `character_ids`, `cosmetic_ids`, `modular_part_ids`, `ability_ids`, `pickup_ids`, `collectible_ids`, `reward_ids` (composite SKUs use `character_id:item_id`). Optional `pickup_rules`. Studios should assign locked entities.
- Activate: avatars/titles only (`active_avatar_id` / `active_title_id` on profile); must own first; achievements never activate
- Collectible images: achievements and avatars upload via multipart (`…/achievements/{id}/image`, `…/avatars/{id}/image`) → max-edge cap + WebP → `image_url`; titles are text-only
- Asset packs (game download): pack parts are `zip` or `raw` archives/binaries; collectible images use the content image pipeline
- Spawn payload: `character` (`stats`, `abilities`, …), `cosmetics`, `modular_parts`, `spawn` (optional `transform`)
- `ShopLine`: `game_code` / `sku_id` = storage **NAME**; `label` = locale-resolved display or null; `price` / nested prices; `owned`; `meta`
- Shop access: `GET /apps/games/shop/access?game_code&sku_kind&sku_id&provider?` → `{ locked, owned, store_product_id? }`. `GET …/access/service` is DB ownership for game servers (email/phone + service key); optional `purchase_token`+`provider` validates with the store and persists owned. UE **Get Shop Sku Access** / Unity `GetShopSkuAccessAsync` query native inventory then **POST unlock/restore** so Fast Game `content_ownership` matches Myket/Bazaar/Play. ZarinPal/Steam: Fast Game ownership only.
- Catalog/content/shop: `?lang=` (or `Accept-Language`) resolves display `label`; `?expand_i18n=true` returns full `translations` (default omitted). Clients never send labels on buy/claim/access.
- **SDK / Blueprint**: pass `Lang` (e.g. `fa`) on **List Games**, **Get Game**, **Get Shop Catalog**, **List Characters**, **Get Map Runtime**, **Resolve Spawn**, **Prepare Session**. Optional **Expand I18n** for the full translations map. Empty `Lang` = server default.

## Entity locales

**Canonical doc:** [fast-game/docs/entity-locales.md](../fast-game/docs/entity-locales.md)

Shell UI is English-only. Catalog **entity copy** uses optional open `translations`:

```json
{ "en": { "name": "…", "description": "…" }, "de": {…} }
```

| Rule | Value |
|------|--------|
| NAME | Locale-free `*_id` / event `name` / shop codes — never localized |
| translations | Optional; any valid lang tag — **editor writes**; SDKs/playground **read only** (request with `expand_i18n=true`) |
| label | Resolved at read (`?lang=` / Accept-Language / SDK `Lang` pin) or null — never send on buy/claim |
| Resolve | requested → `en` → any → null |

**With `translations`:** game catalog, maps, characters, pickups, achievements, avatars, titles.

**Without:** **events** (`name` only — never `translations`), cosmetics/abilities/modular parts, legacy collectible grant defs, shop price rows (prices only — copy comes from the SKU’s entity).

Auth: UE **Is Authenticated** / **Check Authentication** (`bAuthenticated`); Unity `IsAuthenticated` / `CheckAuthenticationAsync()`; panel `$authStore.isAuthenticated`.

Unity / Unreal must **parse** open `translations` maps when `expand_i18n` is set, otherwise use resolved `label`. Must **not** invent a second locale map or POST translations. Panel **editor** uses `LocaleFields` + `shared/locales.ts` to write locales.
Panel routes use **NAME** (`game_id`), never display labels.
## Breaking change — NPCs removed

The standalone NPC entity is gone: no `game_npc` table, no `/npcs` routes, no `map_npc_placement`.

- Author NPCs as a `GameCharacter` with `role: "npc"` (or `"both"`); list them with
  `?role=npc`.
- Place them via `character-placements` (keyed by `character_id`, optional `kind`:
  `player` | `npc`) instead of `npc-placements`. Prefer placements over raw `MapPlayerSpawn`
  for authored spawn points (`kind: "player"` + `mode_id` + `team`).
- Map runtime returns `character_placements[]` in place of `npcs[]`; each entry carries
  `placement_id`, `character_id`, `kind`, `role`, `label`, `stats`, `overrides`, `mode_id`,
  `team`, optional `transform`. Grouped views: `character_placements_by_mode`,
  `pickups_by_mode`. The old `disposition` field is gone — express hostility via `stats` /
  `overrides`.

## Verification rule

Any change to these contracts must be verified against **`fast-game/frontend/`** (playground / panel APIs) and **`fast-game-unity`** + **`fast-game-ue`**. Run `py -3 run_tests.py` in each SDK project.
