# FastGameStore (Unreal)

Internal Android store OS for Fast Game. Enable with **Fast Game**. Designers should **not** use this subsystem — call **Unlock Sku** on Fast Game.

Android flavors live in this plugin (`Source/FastGameStore/Java/`):

| Flavor | Path | Store app package | `StorePlatform` |
|--------|------|-------------------|-----------------|
| Myket | `Java/myket/FastGameStoreActivity.java` | `ir.mservices.market` | `myket` |
| Cafe Bazaar | `Java/cafebazaar/FastGameStoreActivity.kt` | `com.farsitel.bazaar` | `caffebazar` |
| Google Play | `Java/googleplay/FastGameStoreActivity.java` | `com.android.vending` | `googleplay` |

1. Copy this plugin next to `Plugins/FastGame`.
2. Set `FastGameStore.Build.cs` `StoreFlavor` to **Myket**, **CafeBazaar**, or **GooglePlay** (one APK).
3. **Initialize Game** `StorePlatform` must match (`myket` / `caffebazar` / `googleplay`). OS install check (1x). Then **Initialize Client** (`ApiBaseUrl`) for network / reconnect (Nx). Does not wipe token or Enter identity.
4. **Do not** paste Cafe Bazaar / Myket RSA in Unreal. Set it in Fast Game Editor payment config. After login the SDK fetches a wrapped copy (`store-verify-key`) and forwards it as `storePublicKey`. Optional Blueprint **Set Store Public Key** is a local override only.
5. Cafe Bazaar `store_skus.caffebazar` must be the **console SKU** (the old Polarise APK used `LittleGuardiansGame`). Do **not** put Fast Game map ids (`full_game` / `full_map`) there. Unlock Sku uses Fast Game `sku_kind`/`sku_id` (`map` / `full_game`); the native extra is the mapped store id.
6. Keep **one Fast Game account**. Store wallet email need not match Fast Game phone — restore binds the receipt to the logged-in Fast Game user.
7. After login: Fast Game **Unlock Sku** (`SkuKind`, `SkuId`; empty GameCode → client).
8. Branch `bOwned`. Optional UI: **Get Shop Sku Access**. ZarinPal/Steam: **Complete Unlock** after return/overlay.

Missing store app → fail, never fake owned. Never kill the process. **Get Shop Sku Access** calls inventory with `openTheStorePage=false` and restores Fast Game ownership when the store already owns the SKU.

Intent extras: `openTheStorePage`, `storeProductId`, optional `storePublicKey`.

JNI: `Java_com_fastgame_store_FastGameStoreActivity_OnStorePurchase` — `FString` by value on the delegate. APL has no raw `&`.

## iOS

See [`iOS/README.md`](iOS/README.md) (StoreKit later; same Fast Game owned grant).
