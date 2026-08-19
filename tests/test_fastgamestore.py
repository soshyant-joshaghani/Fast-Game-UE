"""FastGameStore SDK tests — Unreal Android flavors and OS surfaces."""
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
UE_STORE = ROOT / "Plugins/FastGameStore"
UE_JAVA = UE_STORE / "Source/FastGameStore/Java"

FLAVORS = {
    "myket": ("myket/FastGameStoreActivity.java", "ir.mservices.market", "isMyketInstalled"),
    "cafebazaar": ("cafebazaar/FastGameStoreActivity.kt", "com.farsitel.bazaar", "isMarketAppInstalled"),
    "googleplay": ("googleplay/FastGameStoreActivity.java", "com.android.vending", "isPlayStoreInstalled"),
}


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def test_android_flavors_present():
    assert UE_JAVA.is_dir(), f"android flavor root missing: {UE_JAVA}"
    for rel, _pkg, _fn in FLAVORS.values():
        src = UE_JAVA / rel
        assert src.is_file(), f"missing {rel}"


def test_android_install_checks_and_no_kill():
    myket = _read(UE_JAVA / "myket/FastGameStoreActivity.java")
    bazaar = _read(UE_JAVA / "cafebazaar/FastGameStoreActivity.kt")
    play = _read(UE_JAVA / "googleplay/FastGameStoreActivity.java")
    assert "ir.mservices.market" in myket and "isMyketInstalled" in myket
    assert "com.farsitel.bazaar" in bazaar and "isMarketAppInstalled" in bazaar
    assert "com.android.vending" in play and "isPlayStoreInstalled" in play
    for src in (myket, bazaar, play):
        assert "OnStorePurchase" in src
        assert "openTheStorePage" in src
        assert "storeProductId" in src
        assert "killProcess" not in src
        assert "exitProcess" not in src
    assert "native void OnStorePurchase" in myket
    assert "native void OnStorePurchase" in play
    assert "external fun OnStorePurchase" in bazaar
    assert "purchaseToken" in bazaar
    assert "storePublicKey" in myket and "storePublicKey" in bazaar
    assert "Loading ..." in bazaar
    assert "normalizeRsaPublicKey" in bazaar
    assert "getSubscribedProducts" in bazaar
    assert "retry without local RSA" in bazaar
    assert "re-query inventory after Bazaar purchase UI" in bazaar
    assert "disconnected {" in bazaar and "finish()" not in bazaar.split("disconnected {")[1].split("}")[0]


def test_ue_store_is_os_only():
    header = _read(UE_STORE / "Source/FastGameStore/Public/FastGameStoreSubsystem.h")
    cpp = _read(UE_STORE / "Source/FastGameStore/Private/FastGameStoreSubsystem.cpp")
    assert "PurchaseOrRestoreStoreSku" not in header
    assert "RequestPurchaseToken" in header
    assert "QueryStoreOwnership" in header
    assert "IFastGameNativeStore" in header
    assert "FFastGameNativeStore::Register" in cpp
    assert "Java_com_fastgame_store_FastGameStoreActivity_OnStorePurchase" in cpp
    assert "LaunchFastGameStore" in cpp
    assert "com.farsitel.bazaar" in cpp
    assert "ir.mservices.market" in cpp
    assert "com.android.vending" in cpp
    build = _read(UE_STORE / "Source/FastGameStore/FastGameStore.Build.cs")
    assert '"FastGame"' in build


def test_ue_apl_xml():
    apl_dir = UE_STORE / "Source/FastGameStore"
    for name in (
        "FastGameStore_Myket_APL.xml",
        "FastGameStore_CafeBazaar_APL.xml",
        "FastGameStore_GooglePlay_APL.xml",
    ):
        src = _read(apl_dir / name)
        assert "com.fastgame.store.FastGameStoreActivity" in src
        assert " && " not in src
