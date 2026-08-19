"""Fast Game UE SDK + FastGameStore tests.

Run from project root:
  py -3 run_tests.py
"""
from __future__ import annotations

import importlib.util
import subprocess
import sys
import traceback
from pathlib import Path

ROOT = Path(__file__).resolve().parent
TESTS = ROOT / "tests"


def _load(mod_name: str, path: Path):
    spec = importlib.util.spec_from_file_location(mod_name, path)
    mod = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(mod)
    return mod


def _run_module_tests(mod) -> tuple[int, int]:
    failed = 0
    ran = 0
    for name in sorted(n for n in dir(mod) if n.startswith("test_")):
        fn = getattr(mod, name)
        if not callable(fn):
            continue
        ran += 1
        try:
            fn()
            print(f"  PASS {name}")
        except Exception as exc:
            failed += 1
            print(f"  FAIL {name}: {exc}")
            traceback.print_exc()
    return ran, failed


def _run_script(name: str) -> int:
    path = TESTS / name
    print(f"\n== compile {name} ==")
    return subprocess.call([sys.executable, str(path)])


def main() -> int:
    print("== Fast Game SDK contract (Unreal) ==")
    fg = _load("test_fastgame_sdk", TESTS / "test_fastgame_sdk.py")
    ran, failed = _run_module_tests(fg)

    print("\n== FastGameStore SDK contract (Unreal) ==")
    store = _load("test_fastgamestore", TESTS / "test_fastgamestore.py")
    r2, f2 = _run_module_tests(store)
    ran += r2
    failed += f2

    if failed:
        print(f"\n{failed}/{ran} contract tests failed - skip compile")
        return 1

    print(f"\n{ran} contract tests passed")
    rc = _run_script("compile_android_store.py")
    if rc != 0:
        return rc

    print("\nALL Fast Game UE SDK tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
