# ApiOnly sample (Unreal)

1. Enable FastGame; depend on `FastGame` in your module `Build.cs` (C++ path).
2. Use [`FastGameApiOnlyExample.h`](FastGameApiOnlyExample.h) from your game module.
3. Call `FastGameRunApiOnlyDemo(...)`.

FastAPI only — no Colyseus package required.

## Blueprint

Prefer **`UFastGameSubsystem`** (Get Game Instance → Get Subsystem → **Fast Game**):

1. **Initialize Game** (`GameCode` + `StorePlatform`) then **Initialize Client** with `http://api.localhost/api/v1` (include scheme + `/api/v1`)
2. Bind **On Login Complete** / **On Prepare Session Complete** / **On Shop Catalog Complete** first
3. **Login** → wait for **On Login Complete** → **Prepare Session** → **Get Shop Catalog**

Wrong: `Api Base Url = api.localhost` → server sees `POST /base/login/access-token` → 404.

No `.uasset` sample ships with the plugin; the subsystem nodes are available after enabling Fast Game and rebuilding.

Shop (C++):

```cpp
// Amount comes back from the server — do not send a client price.
Client->Shop->UnlockSku(GameCode, SkuKind, SkuId, CallbackUrl, TEXT(""), ...);
Client->Shop->CompleteUnlock(TEXT(""), ...);
```

Shop (Blueprint): optional **Get Shop Sku Access** (NAMEs) → Branch `bOwned` / `bLocked`. **Unlock Sku**. **Event Shop Progress** from BeginPlay (Purchase Successful / Pending / Failed / Cancelled / Store Missing) fires from anywhere in the flow. Free items use **Claim Free**. ZarinPal **Callback Url**: `http://dashboard.localhost/verify`. **Initialize Game** fails with a store-name message if Cafe Bazaar / Myket is not installed.

Auth (Blueprint): **Is Authenticated** (pure, token present) or **Check Authentication** (latent, validates `/me`, clears bad token) → Branch on `bAuthenticated`.
