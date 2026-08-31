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
    for removed in (
        "Buy With Provider",
        "Submit Billing",
        "VerifyPending",
        "FinalizeSteam",
        'DisplayName = "Buy"',
    ):
        assert removed not in header


def test_ue_initialize_game_vs_client():
    header = _read(UE / "Source/FastGame/Public/FastGameSubsystem.h")
    cpp = _read(UE / "Source/FastGame/Private/FastGameSubsystem.cpp")
    assert "Initialize Game" in header
    assert "void UFastGameSubsystem::InitializeGame" in cpp
    assert "InitializeClientAndGame" not in header
    assert "void UFastGameSubsystem::InitializeClientAndGame" not in cpp
    game_fn = cpp[
        cpp.find("void UFastGameSubsystem::InitializeGame") : cpp.find(
            "void UFastGameSubsystem::InitializeClient("
        )
    ]
    client_fn = cpp[
        cpp.find("void UFastGameSubsystem::InitializeClient(") : cpp.find(
            "void UFastGameSubsystem::SetStorePublicKey"
        )
    ]
    assert "EnsureStoreSetup" in game_fn
    assert "EnsureStoreSetup" not in client_fn
    assert "LoadPersistedAccessToken" in client_fn
    assert "CarryToken" in client_fn
    assert "ClearEnteredIdentity" not in client_fn
    assert "Logout" not in client_fn


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


def test_ue_auth_shop_scenario_exec_pins():
    types = _read(UE / "Source/FastGame/Public/FastGameBlueprintTypes.h")
    header = _read(UE / "Source/FastGame/Public/FastGameSubsystem.h")
    cpp = _read(UE / "Source/FastGame/Private/FastGameSubsystem.cpp")
    latent = _read(UE / "Source/FastGame/Private/FastGameLatentActions.h")

    assert "EFastGameRequestOutcome" in types
    assert "EFastGameEnterPin" in types
    assert "EFastGameAuthCheck" in types
    assert "EFastGameShopAccessRoute" in types
    assert 'EnterPassword UMETA(DisplayName = "Enter Password")' in types
    assert 'Signup UMETA(DisplayName = "Signup")' in types
    assert "Owned UMETA" in types and "Available UMETA" in types
    assert 'Authenticated UMETA(DisplayName = "Authenticated")' in types

    assert 'ExpandEnumAsExecs = "Pin"' in header
    assert 'DisplayName = "Enter"' in header
    assert 'ExpandEnumAsExecs = "Outcome"' in header
    assert 'ExpandEnumAsExecs = "Check"' in header
    assert 'DisplayName = "Login"' in header
    assert 'DisplayName = "Send Auth Code"' in header
    assert 'DisplayName = "Update Full Name"' in header
    assert 'DisplayName = "Unlock Sku"' in header
    assert 'ExpandEnumAsExecs = "Progress"' in header
    assert 'ExpandEnumAsExecs = "Access"' in header
    assert 'DisplayName = "Get Shop Sku Access"' in header

    # Legacy Blueprint nodes removed (C++ client may still have CompleteAccount)
    assert 'DisplayName = "Complete Account"' not in header
    assert "Request Password Recovery" not in header
    assert "Request Signup Verification" not in header
    assert "DeprecatedFunction" not in header

    # Scenario nodes: no redundant bool& bSuccess / bOwned / bLocked pins
    login = header[header.find("void Login(") : header.find("void ClearEnteredIdentity(")]
    assert "bool& bSuccess" not in login
    unlock = header[header.find("void UnlockSku(") : header.find("void CompleteUnlock(")]
    assert "bool& bSuccess" not in unlock
    assert "bool& bOwned" not in unlock
    access = header[
        header.find("void GetShopSkuAccess(") : header.find("bool IsShopLineLocked(")
    ]
    assert "bool& bLocked" not in access
    assert "bool& bOwned" not in access
    assert "bool& bSuccess" not in access

    assert "EnterRouteToPin" in cpp
    assert "ClassifyShopAccess" in cpp
    assert "LastEnterRoute == EFastGameEnterRoute::CompleteAccount" in cpp
    assert "Client->Auth->CompleteAccount" in cpp
    assert "OutcomeOut" in latent
    assert "EnterPinOut" in latent
    assert "AuthCheckOut" in latent
    assert "ShopAccessRouteOut" in latent


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


def test_ue_tip_facade_content_methods():
    header = _read(UE / "Source/FastGame/Public/FastGameClient.h")
    cpp = _read(UE / "Source/FastGame/Private/FastGameClient.cpp")
    sub_h = _read(UE / "Source/FastGame/Public/FastGameSubsystem.h")
    contract = _read(ROOT / "CONTRACT.md")
    assert "void GetBootstrap" in header
    assert "void GetGameConfig" in header
    assert "void GetMapConfig" in header
    assert "void GetCharacter" in header
    assert "void GetDialogue" in header
    assert "void GetQuiz" in header
    assert "void GetStrings" in header
    assert "void FFastGameContent::GetBootstrap" in cpp
    assert "void FFastGameContent::GetGameConfig" in cpp
    assert "void FFastGameContent::GetMapConfig" in cpp
    assert "void FFastGameContent::GetCharacter" in cpp
    assert "void FFastGameContent::GetDialogue" in cpp
    assert "void FFastGameContent::GetQuiz" in cpp
    assert "void FFastGameContent::GetStrings" in cpp
    assert "/apps/games/tip/" in cpp
    assert "/apps/games/strings/" in cpp
    assert "/bootstrap" in cpp
    assert 'TEXT("/game")' in cpp or "/game" in cpp
    assert "/maps/" in cpp
    assert "void UFastGameSubsystem::GetBootstrap" in _read(
        UE / "Source/FastGame/Private/FastGameSubsystem.cpp"
    )
    assert 'DisplayName = "Get Bootstrap"' in sub_h
    assert 'DisplayName = "Get Game Config"' in sub_h
    assert 'DisplayName = "Get Map Config"' in sub_h
    assert "GetBootstrap" in contract
    assert "GetGameConfig" in contract
    assert "GetMapConfig" in contract
    assert "GetPackTip" in header
    assert "/apps/games/asset-packs/" in cpp
    assert "deprecated for players" in contract.lower()


def test_ue_download_pack_filter_and_platform():
    types = _read(UE / "Source/FastGame/Public/FastGameTypes.h")
    bp_types = _read(UE / "Source/FastGame/Public/FastGameBlueprintTypes.h")
    selector = _read(UE / "Source/FastGame/Public/FastGamePackSelector.h")
    platform = _read(UE / "Source/FastGame/Public/FastGameRuntimePlatform.h")
    download = _read(UE / "Source/FastGame/Public/FastGameDownloadSceneComponent.h")
    sub_h = _read(UE / "Source/FastGame/Public/FastGameSubsystem.h")
    client = _read(UE / "Source/FastGame/Private/FastGameClient.cpp")

    assert "TArray<FString> Quality" in types
    assert "TArray<FString> Platforms" in types
    assert "TArray<FString> Languages" in types
    assert "Kind" in types
    assert "TArray<FString> Quality" in bp_types

    assert "FFastGamePackSelector" in selector
    assert "MatchesTagList" in selector
    assert "GetRuntimeOs" in platform
    assert "GetQualityClass" in platform
    assert "StorePlatformToOs" in platform

    assert "RunDownload" in download
    assert "Tip not published" in _read(UE / "Source/FastGame/Private/FastGameDownloadSceneComponent.cpp")
    assert "ListPacksFromGameTip" in client
    assert 'TEXT("payload")' in client or 'TEXT("asset_packs")' in client
    assert "GetPackTip" in client
    assert "Filter Packs For Download" in sub_h
    assert "Get Preferred Language" in sub_h
    assert "List Packs From Game Config Json" in sub_h


def test_ue_realtime_joinmap_seat():
    header = _read(UE / "Source/FastGame/Public/FastGameClient.h")
    cpp = _read(UE / "Source/FastGame/Private/FastGameClient.cpp")
    types = _read(UE / "Source/FastGame/Public/FastGameTypes.h")
    contract = _read(ROOT / "CONTRACT.md")
    assert "class FASTGAME_API FFastGameRealtime" in header
    assert "void MintSeat" in header
    assert "void JoinMap" in header
    assert "void FFastGameRealtime::MintSeat" in cpp
    assert "/apps/games/realtime/seat" in cpp
    assert "struct FASTGAME_API FFastGameSeatMint" in types
    assert "TSharedRef<FFastGameRealtime> Realtime" in header
    assert "Realtime.JoinMap" in contract
    assert "/apps/games/realtime/seat" in contract
    assert "designer-chosen" in contract.lower() or "designer-chosen" in contract


def test_ue_map_hub_and_engine_scene():
    types = _read(UE / "Source/FastGame/Public/FastGameTypes.h")
    bp_types = _read(UE / "Source/FastGame/Public/FastGameBlueprintTypes.h")
    client = _read(UE / "Source/FastGame/Private/FastGameClient.cpp")
    convert = _read(UE / "Source/FastGame/Private/FastGameBlueprintConvert.cpp")
    menu_h = _read(UE / "Source/FastGame/Public/FastGameMenuSceneComponent.h")
    menu_cpp = _read(UE / "Source/FastGame/Private/FastGameMenuSceneComponent.cpp")
    scene_names = _read(UE / "Source/FastGame/Public/FastGameSceneNames.h")

    assert "FFastGameMapRuntimeSettings" in types
    assert "EngineScene" in types
    assert "MapKind" in types
    assert "HubMapIds" in types
    assert "AbilityAllowlist" in types
    assert "FFastGameBPMapRuntimeSettings" in bp_types
    assert "HubMapIds" in bp_types
    assert "ParseMapKind" in client
    assert "ParseMapRuntimeSettings" in client
    assert "ParseMap(" in client
    assert "hub_map_ids" in client
    assert "runtime_settings" in client
    assert "Out.MapKind = In.MapKind" in convert
    assert "Out.HubMapIds = In.HubMapIds" in convert
    assert "OpenLevelFromMap" in menu_h
    assert "OpenLevelFromMap" in menu_cpp
    assert "engine_scene is not configured" in menu_cpp
    assert "DefaultLevelScene" not in menu_h
    assert "LevelSample" not in scene_names


def test_ue_progress_get_save():
    header = _read(UE / "Source/FastGame/Public/FastGameClient.h")
    cpp = _read(UE / "Source/FastGame/Private/FastGameClient.cpp")
    contract = _read(ROOT / "CONTRACT.md")
    assert "class FASTGAME_API FFastGameProgress" in header
    assert "void FFastGameProgress::Get" in cpp
    assert "void FFastGameProgress::Save" in cpp
    assert "/apps/games/progress/" in cpp
    assert "TSharedRef<FFastGameProgress> Progress" in header
    assert "client.Progress" in contract
    assert "/apps/games/progress/" in contract


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


def test_ue_entity_components_and_flow_pins():
    types = _read(UE / "Source/FastGame/Public/FastGameBlueprintTypes.h")
    char_h = _read(UE / "Source/FastGame/Public/FastGameCharacterComponent.h")
    char_cpp = _read(UE / "Source/FastGame/Private/FastGameCharacterComponent.cpp")
    map_h = _read(UE / "Source/FastGame/Public/FastGameMapComponent.h")
    map_cpp = _read(UE / "Source/FastGame/Private/FastGameMapComponent.cpp")
    avatar_h = _read(UE / "Source/FastGame/Public/FastGameAvatarComponent.h")
    title_h = _read(UE / "Source/FastGame/Public/FastGameTitleComponent.h")
    ach_h = _read(UE / "Source/FastGame/Public/FastGameAchievementComponent.h")
    sub_h = _read(UE / "Source/FastGame/Public/FastGameSubsystem.h")
    sub_cpp = _read(UE / "Source/FastGame/Private/FastGameSubsystem.cpp")
    contract = _read(ROOT / "CONTRACT.md")

    assert "EFastGameTravelMapPin" in types
    assert "EFastGameQuestPin" in types
    assert 'Traveled UMETA(DisplayName = "Traveled")' in types
    assert 'Matchmaking UMETA(DisplayName = "Matchmaking")' in types
    assert 'WaitingHere UMETA(DisplayName = "Waiting Here")' in types
    assert 'Complete UMETA(DisplayName = "Complete")' in types
    assert 'NotStartedYet UMETA(DisplayName = "Not Started Yet")' in types
    assert "FFastGameBPSeatMint" in types

    for path in (
        "Source/FastGame/Public/FastGameCharacterComponent.h",
        "Source/FastGame/Private/FastGameCharacterComponent.cpp",
        "Source/FastGame/Public/FastGameMapComponent.h",
        "Source/FastGame/Private/FastGameMapComponent.cpp",
        "Source/FastGame/Public/FastGameAvatarComponent.h",
        "Source/FastGame/Private/FastGameAvatarComponent.cpp",
        "Source/FastGame/Public/FastGameTitleComponent.h",
        "Source/FastGame/Private/FastGameTitleComponent.cpp",
        "Source/FastGame/Public/FastGameAchievementComponent.h",
        "Source/FastGame/Private/FastGameAchievementComponent.cpp",
    ):
        assert (UE / path).is_file(), path

    assert "CharacterId" in char_h
    assert "Fetch Character" in char_h
    assert "OnCharacterFetched" in char_h
    assert "GetCharacter" in char_cpp

    assert "MapId" in map_h and "ModeId" in map_h
    assert 'DisplayName = "Get Map Config"' in map_h
    assert 'ExpandEnumAsExecs = "Pin"' in map_h
    assert 'DisplayName = "Travel Map"' in map_h
    assert "OnQuestComplete" in map_h
    assert "OnQuestFailed" in map_h
    assert "OnQuestNotStartedYet" in map_h
    assert "NotifyQuestComplete" in map_h
    assert "JoinMap" in map_cpp or "MintSeat" in map_cpp
    assert "OpenLevel" in map_cpp

    assert "AvatarId" in avatar_h
    assert "TitleId" in title_h
    assert "AchievementId" in ach_h

    assert 'DisplayName = "Get Character"' in sub_h
    assert "void UFastGameSubsystem::GetCharacter" in sub_cpp
    assert "OnGetCharacterComplete" in sub_h

    assert "FastGameCharacterComponent" in contract
    assert "FastGameMapComponent" in contract
    assert "Travel Map" in contract
    assert "sdk-pin-policy.md" in contract
    assert "Not Started Yet" in contract
