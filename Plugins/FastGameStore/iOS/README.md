# Fast Game iOS store util (later)

Official upcoming util — **not implemented this round**.

Same contract as Android **FastGameStore** in this Unreal plugin:

1. Author the item once in Fast Game (e.g. map `full_game`).
2. Create the matching App Store IAP.
3. Map `store_skus.appstore` on the Fast Game price row (when backend lands).
4. Initialize Game `StorePlatform = appstore`.
5. Unlock Sku → StoreKit → Fast Game unlock/complete → `owned=true`.

Fast Game remains the **only** ownership source of truth. Until StoreKit ships, iOS builds use Fast Game HTTP only (ZarinPal / other enabled providers).
