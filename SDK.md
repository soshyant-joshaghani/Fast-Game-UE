# Fast Game SDK — Unreal Engine

Plugin: `Plugins/FastGame` — **FastAPI client only**.

## Install

Plugins are **pre-installed** in this project (`Plugins/FastGame`, `Plugins/FastGameStore`).

To use in another UE project:

1. Copy `Plugins/FastGame` into your project `Plugins/`.
2. For Android IAP, also copy `Plugins/FastGameStore` (flavors under `Source/FastGameStore/Java/`). See [Plugins/FastGameStore/README.md](Plugins/FastGameStore/README.md). iOS later: [Plugins/FastGameStore/iOS/README.md](Plugins/FastGameStore/iOS/README.md).
3. Enable **Fast Game**, add `"FastGame"` to your module `PublicDependencyModuleNames` (C++ usage).
4. Rebuild the project.

## Blueprint usage

The plugin exposes **`UFastGameSubsystem`** (Game Instance subsystem, display name **Fast Game**).

1. **Get Game Instance** → **Get Subsystem** → **Fast Game**
2. **Initialize Game** (1x) with **Game Code** and **Store Platform** (this APK’s store: Cafe Bazaar / Myket / Google Play / Steam / ZarinPal). Runs OS install check (`bSuccess` / `Message`); does not wipe token or Enter identity. Editor / non-Android skips the install check.

3. **Initialize Client** (Nx, reconnect) with the **full** API base URL only:

   `http://api.localhost/api/v1`

   Not `api.localhost` alone — without `/api/v1` the backend logs `POST /base/login/access-token` → **404**.
   (Host-only values are auto-normalized by the plugin, but prefer the full URL.)
   Auth OTP / recovery / signup-verify use **Initialize Game** `GameCode` internally — **Enter / Login / Signup have no Game Code pin**.
   Empty shop **Provider** / **GameCode** pins use Initialize Game. **Enter** alone persists identity for empty Login/Signup/Recovery IDs (Logout / Clear wipe it). Fast Game identity (e.g. phone) need **not** match the Myket/Cafe Bazaar wallet email — ownership is Fast Game user + store receipt.

4. Bind optional delegates if needed, then call **Login** / **Signup** / **Get Me**, catalog, content, or shop nodes.
   **All HTTP Blueprint calls are latent** (clock icon): execution waits for the response, then continues with **b Success**, **Status Code**, and **Message** (parsed API `detail` when present), plus method-specific outputs (games, session, payment, etc.).
5. After a successful Branch, chain further latent calls (e.g. **Prepare Session**, **Get Shop Catalog**).
   Catalog / shop / character / map / spawn / prepare nodes take optional **Lang** (e.g. `fa`) for resolved labels and **Expand I18n** for full `translations` maps (Advanced pins).

### Auth notes

- **Enter** (ENTER contract): latent node with exec pins **Login** / **Complete Account** / **Verify Id** / **Register** / **Failed** (**breaking** — refresh the Enter node). Inputs: `Identity`, **Channel** enum (`Auto` / `Email` / `Phone`). On success, **stores** `OutIdentity` under `Saved/FastGame/FastGameEnteredIdentity.txt` and sets **Last Enter Route**. Outputs: `OutIdentity`, `OutEmail` / `OutPhone`, `bOutEmail` / `bOutPhone`, `bSuccess`, `Message`. **No widgets**.
  - exists + has password → **Login**
  - exists + no password → **Complete Account** (no OTP)
  - new + verify ON for client GameCode → **Verify Id**
  - new + verify OFF → **Register**
- **Forgot is not an Enter pin.** Put **Begin Forgot** on the Login widget; then **Send Auth Code** / **Verify Auth Code** use recovery OTP and **Set Password** (`Confirm Password Recovery`).
- **Login** / **Register** (`Signup`) / **Complete Account** / recovery / signup OTP: leave **Identity** (or Email+Phone) **empty** to use the ENTER-stored identity — same as Unity.
- **Send Auth Code** / **Verify Auth Code**: empty Identity → ENTER store; **Verify Id** → signup OTP; **Begin Forgot** → recovery OTP; otherwise fail clearly. Do not call recovery for new users or Register for existing users.
- **Login** takes `Identity` (optional), `Password`, **Channel**. Empty `Identity` → ENTER store.
- **Register** (`Signup`) takes **Email** / **Phone** (optional after Enter), **Password** + **Password Confirm**, optional **Full Name**. Both Email+Phone empty → ENTER store. New users only.
- **Complete Account** takes **Password** + **Password Confirm**, optional **Full Name** — passwordless existing users only (no OTP).
- **Update Full Name** (`PATCH /me`) after login — display name only.
- **Request / Verify / Confirm Password Recovery**: empty `Identity` → ENTER store. Confirm has **no Code pin** and **no Full Name**. Use after **Begin Forgot** only.
- **Clear Entered Identity** clears the store and forgot flag; **Clear Local Cache** also clears it.
- Helpers: **Is Email Identity** / **Is Phone Identity**. **Set Game Code** / **Get Game Code** if the active title changes after init.

Example flow:

```text
InitializeGame(GameCode, StorePlatform) → InitializeClient(ApiBaseUrl) → Enter(Identity, Channel=Auto) → stores OutIdentity + LastEnterRoute
  Login            → Login(/*Identity empty*/, Password, Channel)
                   → Begin Forgot → Send Auth Code → Verify Auth Code → Set Password
  Complete Account → Complete Account(/*empty*/, Password, Confirm, Full Name)
  Verify Id        → Send Auth Code(/*empty*/) → Verify Auth Code(/*empty*/, Code) → Register
  Register         → Signup(/*Email+Phone empty*/, Password, Confirm, Full Name)
Clear Entered Identity when leaving auth / switching accounts
```

```text
InitializeGame → InitializeClient → Enter / Login / Signup → **Is Authenticated** or **Check Authentication** → Branch
  → Branch(bSuccess) → GetMe (latent) → Branch → bind User / CurrentUser to widgets
  → PrepareSession (latent) → Branch → Session.ColyseusRoom, MapRuntimeJson, SpawnJson
  → GetGameServer (latent) → sibling Colyseus JoinOrCreate
```

Forgot password (Login screen only, 3 steps):

```text
BeginForgot → RequestPasswordRecovery(/*Identity empty after Enter*/)
           → VerifyPasswordRecovery(/*Identity empty*/, Code)
           → ConfirmPasswordRecovery(/*Identity empty*/, NewPassword, NewPasswordConfirm)
→ auto-login (same as Register)
```

**Register** (`Signup`) and **Complete Account** create/set credentials then log in. All latent HTTP nodes wait for the response then expose **b Success** for Branch; **Last Auth Status Code** / **Last Auth Message** reflect the most recent request.
Dev tool: **Clear Local Cache** wipes the saved access token and pending-payment file.
The access token is saved under `Saved/FastGame/FastGameAccessToken.txt` and restored on **Initialize Client**. **Logout** / **Clear Local Cache** delete it.

Nested JSON (`TranslationsJson`, `MapRuntimeJson`, `SpawnJson`, …) is returned as strings — parse in Blueprint with JSON utilities or treat as opaque.

## C++ usage

`#include "FastGameClient.h"`

```cpp
FFastGameConfig Config;
Config.ApiBaseUrl = TEXT("http://api.localhost/api/v1");
Config.GameCode = TEXT("sandbox-capsule");
TSharedRef<FFastGameClient> Client = MakeShared<FFastGameClient>(Config);

Client->Auth->Enter(Identity, EFastGameIdentityChannel::Auto,
  [](bool bOk, int32 Code, bool bExists, bool bPasswordRequired,
     FString Channel, FString Email, FString Phone, FString Err) {
    // Route: exists+password → Login; exists+no password → CompleteAccount;
    // !exists+verify → VerifyId; !exists → Register
  });
Client->Auth->CompleteAccount(Password, PasswordConfirm, FullName,
  [](bool bOk, int32 Code, FString UserId, FString OutEmail, FString OutPhone, FString Token, FString Err) { /* ... */ });
Client->Auth->Signup(Email, Phone, Password, PasswordConfirm, FullName,
  [](bool bOk, int32 Code, FString UserId, FString OutEmail, FString OutPhone, FString Token, FString Err) { /* logged in on success */ });
Client->Auth->Login(Identity, Password, [](bool bOk, int32 Code, FString Token, FString Err) { /* ... */ });
Client->Auth->UpdateFullName(FullName, [](bool bOk, int32 Code, FFastGameUser User, FString Err) { /* ... */ });
// Forgot from Login screen — 3 steps
Client->Auth->RequestPasswordRecovery(Identity, [](bool bOk, int32 Code, FString Err) { /* OTP sent */ });
Client->Auth->VerifyPasswordRecovery(Identity, Code, [](bool bOk, int32 Status, FString Err) {
  /* on success: show password panel */ });
Client->Auth->ConfirmPasswordRecovery(Identity, TEXT("") /* already verified */, NewPassword, NewPasswordConfirm,
  [](bool bOk, int32 Code, FString Err) { /* password updated */ });
Client->Content->PrepareSession(TEXT("sandbox-capsule"), TEXT("sandbox"), TEXT("box-arena"),
  [](bool bOk, FFastGamePreparedSession Session, FString Err) { /* ... */ });
```

Blueprint facade: `#include "FastGameSubsystem.h"` and get the subsystem from `UGameInstance`.

### Shop — Android Myket / Cafe Bazaar / Google Play

Also copy **`Plugins/FastGameStore`**. Set `FastGameStore.Build.cs` flavor to match **Initialize Game** StorePlatform (one APK per store).

Designers use **Unlock Sku** only (FastGameStore is automatic). For store builds:

```text
InitializeGame(GameCode, CafeBazaar)   // or Myket / GooglePlay — OS install check (1x)
→ InitializeClient(ApiBaseUrl)         // network / reconnect (Nx)
→ (existing auth: Enter then Login empty ID, or Login with filled ID)
   Fast Game phone + store email wallet is fine
   RSA is fetched from Editor after login — leave Set Store Public Key empty
→ Unlock Sku (map, full_game, CallbackUrl empty on store APKs)
→ Branch(bOwned) → Open Level
```

ZarinPal/Steam: Unlock Sku then **Complete Unlock** after return/overlay. `caffebazar` checks Cafe Bazaar (`com.farsitel.bazaar`). Missing store → `bSuccess=false`, never fake owned. Flavors: `Plugins/FastGameStore/Source/FastGameStore/Java/`. See [Plugins/FastGameStore/README.md](Plugins/FastGameStore/README.md). iOS later: [Plugins/FastGameStore/iOS/README.md](Plugins/FastGameStore/iOS/README.md).

Empty shop **GameCode** / **Provider** pins use **Initialize Game** (same pattern as Enter empty Identity). Unlock Sku has no Provider pin — it always uses Initialize Game StorePlatform. **Get Shop Sku Access** also queries Myket/Cafe Bazaar/Google Play inventory (no purchase UI) and restores Fast Game ownership if the store already owns the SKU. Login freezes the current store wallet (`POST …/shop/store-lock`) so switching Myket/Cafe Bazaar mid-session cannot restore another account’s IAPs. Buy on one store APK → same Fast Game login is `owned` on the other (see [fast-game/docs/sdk-shop-flows.md](../fast-game/docs/sdk-shop-flows.md)).

## Multiplayer (sibling Colyseus)

1. Add [colyseus-unreal](https://github.com/charisma-ai/colyseus-unreal) as a **sibling** plugin (pin a commit SHA).
2. Fast Game: `PrepareSession` + `Catalog->GetGameServer` (or Blueprint **Get Game Server**).
3. Colyseus: `Client::JoinOrCreate(session.ColyseusRoom, { gameId, modeId, mapId })` — see `Samples/SandboxMultiplayer`.

Fast Game does not wrap Colyseus join/send/leave.

## Named modules

See [CONTRACT.md](CONTRACT.md).

### Ads (Blueprint)

After **Initialize Client** + login, use `FastGame|Ads`:

- **Get Image Ad** → `ImageUrl`, `ClickUrl`, size, typed `Ad`
- **Get Video Ad** → `VideoUrl`, `ClickUrl`, `Ad`
- **Get Gif / Lottie / Rive Ad** → `MediaUrl`, `ClickUrl`, `Ad`
- **Get Text Ad** → `Title`, `Body`, `BackgroundUrl`, `BackgroundColor`, `ClickUrl`, `Ad`
- **Track Ad Displayed / Clicked / Closed**
- Pure: **Is Image Ad**, **Break Advertisement**, …

Text creatives store copy in metadata JSON: `title`, `body`, `background_url`, `background_color`.

Media and background URLs from the Ads API are **absolute CDN URLs** (platform / Arvan / Liara / custom public bases). Pass them straight to downloaders / media players — do not prefix with the FastAPI host.

## Samples

- `Samples/ApiOnly` — FastAPI only (C++ sketch + Blueprint subsystem note)
- `Samples/SandboxMultiplayer` — sibling Colyseus join sketch
