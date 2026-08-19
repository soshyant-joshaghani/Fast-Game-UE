"""Fast Game SDK contract tests — Unreal (no engine required)."""
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
UE = ROOT / "Plugins/FastGame"


def _read(rel: Path) -> str:
    return rel.read_text(encoding="utf-8")


def test_ue_unlock_and_ensure_setup():
    header = _read(UE / "Source/FastGame/Public/FastGameSubsystem.h")
    cpp = _read(UE / "Source/FastGame/Private/FastGameSubsystem.cpp")
    shop = _read(UE / "Source/FastGame/Private/FastGameClient.cpp")
    shop_h = _read(UE / "Source/FastGame/Public/FastGameClient.h")
    assert "Unlock Sku" in header
    assert "Complete Unlock" in header
    assert "void UFastGameSubsystem::UnlockSku" in cpp
    assert "void UFastGameSubsystem::CompleteUnlock" in cpp
    assert "EnsureStoreSetup" in cpp
    assert "LoadPersistedAccessToken" in cpp
    assert "CarryToken" in cpp
    assert "/apps/games/shop/unlock/begin" in shop
    assert "/apps/games/shop/unlock/complete" in shop
    assert "void FFastGameShop::UnlockSku" in shop
    assert "void CompleteUnlock" in shop_h
    assert "void FFastGameShop::CompleteUnlock" in shop
    assert "store-verify-key" in shop
    assert "void FFastGameShop::EnsureStoreVerifyKey" in shop
    assert "Unreal Editor cannot return a purchase token" in cpp
    assert (UE / "Source/FastGame/Public/FastGameStoreVerify.h").is_file()
    assert "FastGameUnwrapStoreVerifyKey" in _read(
        UE / "Source/FastGame/Private/FastGameStoreVerify.cpp"
    )
    for deprecated in ("Buy With Provider", "Submit Billing", "VerifyPending", "FinalizeSteam"):
        assert "DeprecatedFunction" in header


def test_ue_initialize_game_vs_client():
    header = _read(UE / "Source/FastGame/Public/FastGameSubsystem.h")
    cpp = _read(UE / "Source/FastGame/Private/FastGameSubsystem.cpp")
    assert "Initialize Game" in header
    assert "void UFastGameSubsystem::InitializeGame" in cpp
    assert "void UFastGameSubsystem::InitializeClientAndGame" in cpp
    game_fn = cpp[
        cpp.find("void UFastGameSubsystem::InitializeGame") : cpp.find(
            "void UFastGameSubsystem::InitializeClient("
        )
    ]
    client_fn = cpp[
        cpp.find("void UFastGameSubsystem::InitializeClient(") : cpp.find(
            "void UFastGameSubsystem::InitializeClientAndGame"
        )
    ]
    assert "EnsureStoreSetup" in game_fn
    assert "EnsureStoreSetup" not in client_fn
    assert "LoadPersistedAccessToken" in client_fn
    assert "CarryToken" in client_fn
    assert "ClearEnteredIdentity" not in client_fn
    assert "Logout" not in client_fn
    assert "DeprecatedFunction" in header
    assert "DeprecationMessage" in header


def test_ue_enter_identity_not_on_initialize_client():
    cpp = _read(UE / "Source/FastGame/Private/FastGameSubsystem.cpp")
    init = cpp[
        cpp.find("void UFastGameSubsystem::InitializeClient(") : cpp.find(
            "void UFastGameSubsystem::SetStorePublicKey"
        )
    ]
    assert "ClearEnteredIdentity" not in init
    assert "Logout" not in init
    auth_h = _read(UE / "Source/FastGame/Public/FastGameClient.h")
    assert "StoreEnteredIdentity" in auth_h
    assert "POST /base/login/enter" in auth_h or "Enter(" in auth_h


def test_ue_store_lock_on_login():
    shop = _read(UE / "Source/FastGame/Private/FastGameClient.cpp")
    header = _read(UE / "Source/FastGame/Public/FastGameClient.h")
    sub = _read(UE / "Source/FastGame/Private/FastGameSubsystem.cpp")
    assert "/apps/games/shop/store-lock" in shop
    assert "void FFastGameShop::BindStoreLock" in shop
    assert "OnLoggedIn" in header
    assert "BindStoreLock" in sub
    ctor = shop[
        shop.find("FFastGameClient::FFastGameClient") : shop.find(
            "void FFastGameAuth::SetAccessToken"
        )
    ]
    assert "BindStoreLock" in ctor
    assert "OnLoggedIn" in ctor


def test_ue_shop_access_queries_store():
    header = _read(UE / "Source/FastGame/Public/FastGameNativeStore.h")
    shop = _read(UE / "Source/FastGame/Private/FastGameClient.cpp")
    cpp = _read(UE / "Source/FastGame/Private/FastGameSubsystem.cpp")
    assert "QueryStoreOwnership" in header
    assert "store_product_id" in shop
    assert "&provider=" in shop or "provider=%s" in shop
    assert "QueryStoreOwnership" in cpp
    access_fn = cpp[
        cpp.find("void UFastGameSubsystem::GetShopSkuAccess") : cpp.find(
            "void UFastGameSubsystem::ClaimFree"
        )
    ]
    assert "RestoreUnlock" in access_fn
    assert "unlock/restore" in shop


def test_ue_shop_empty_pins_use_initialize_game():
    shop = _read(UE / "Source/FastGame/Private/FastGameClient.cpp")
    header = _read(UE / "Source/FastGame/Public/FastGameSubsystem.h")
    assert "OutGameCode = Config.GameCode" in shop
    assert "OutProvider = FastGameNormalizeProviderId(Config.StorePlatform)" in shop
    assert "ResolveGameCode(GameIdFilter" in shop
    assert 'CPP_Default_GameCode = ""' in header
    assert "Unlock Sku" in header
    assert "ResolveProvider(TEXT(\"\")" in shop or 'ResolveProvider(TEXT("")' in shop


def test_ue_native_store_registry_no_hard_depend():
    build = _read(UE / "Source/FastGame/FastGame.Build.cs")
    assert "FastGameStore" not in build
    native = _read(UE / "Source/FastGame/Public/FastGameNativeStore.h")
    assert "RequestPurchaseToken" in native
    assert "QueryStoreOwnership" in native
    assert "EnsureSetup" in native
    assert "IsAndroidStoreProvider" in native


def test_ue_modules_present():
    for rel in (
        "Source/FastGame/Public/FastGameClient.h",
        "Source/FastGame/Public/FastGameSubsystem.h",
        "Source/FastGame/Public/FastGameHttp.h",
        "Source/FastGame/Public/FastGameNativeStore.h",
        "Source/FastGame/Private/FastGameClient.cpp",
        "Source/FastGame/Private/FastGameSubsystem.cpp",
        "Source/FastGame/Public/FastGameStoreVerify.h",
        "Source/FastGame/Private/FastGameStoreVerify.cpp",
    ):
        path = UE / rel
        assert path.is_file(), rel
