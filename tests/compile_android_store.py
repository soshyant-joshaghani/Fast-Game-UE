"""Compile Unreal FastGameStore Android flavors (Myket / Play Java, Cafe Bazaar Kotlin)."""
from __future__ import annotations

import os
import shutil
import subprocess
import urllib.request
import zipfile
from pathlib import Path

TESTS = Path(__file__).resolve().parent
ROOT = TESTS.parent
STUBS = TESTS / "android-stubs"
STUBS_KT = TESTS / "android-stubs-kt"
TOOLS = TESTS / ".tools"
OUT = TESTS / "_compile_out" / "android"
KOTLIN_VERSION = "2.1.20"
KOTLIN_URL = (
    f"https://github.com/JetBrains/kotlin/releases/download/v{KOTLIN_VERSION}/"
    f"kotlin-compiler-{KOTLIN_VERSION}.zip"
)
JAVA_ROOT = ROOT / "Plugins/FastGameStore/Source/FastGameStore/Java"


def _run(cmd: list[str], **kwargs) -> None:
    print("+", " ".join(cmd))
    subprocess.check_call(cmd, **kwargs)


def _which(name: str) -> str | None:
    return shutil.which(name)


def ensure_kotlinc() -> Path:
    existing = _which("kotlinc")
    if existing:
        return Path(existing)
    bat = TOOLS / "kotlinc" / "bin" / "kotlinc.bat"
    sh = TOOLS / "kotlinc" / "bin" / "kotlinc"
    if os.name == "nt" and bat.is_file():
        return bat
    if sh.is_file():
        return sh
    TOOLS.mkdir(parents=True, exist_ok=True)
    zip_path = TOOLS / f"kotlin-compiler-{KOTLIN_VERSION}.zip"
    print(f"Downloading kotlinc {KOTLIN_VERSION}...")
    urllib.request.urlretrieve(KOTLIN_URL, zip_path)
    with zipfile.ZipFile(zip_path) as zf:
        zf.extractall(TOOLS)
    extracted = TOOLS / "kotlinc"
    if not extracted.is_dir():
        raise SystemExit("kotlinc extract failed")
    return bat if os.name == "nt" else sh


def compile_java_stubs(out: Path) -> None:
    javac = _which("javac")
    if not javac:
        raise SystemExit("javac not found")
    sources = sorted(STUBS.rglob("*.java"))
    if not sources:
        raise SystemExit("no java stubs")
    cmd = [javac, "-encoding", "UTF-8", "-d", str(out), *[str(p) for p in sources]]
    _run(cmd)


def compile_java_flavor(out: Path, src: Path, label: str) -> None:
    javac = _which("javac")
    if not src.is_file():
        raise SystemExit(f"missing Java flavor: {src}")
    dest = out / label
    dest.mkdir(parents=True, exist_ok=True)
    _run(
        [
            javac,
            "-encoding",
            "UTF-8",
            "-cp",
            str(out),
            "-d",
            str(dest),
            str(src),
        ]
    )
    print(f"OK javac {label}: {src.name}")


def compile_kotlin_bazaar(out: Path, kotlinc: Path, src: Path, label: str) -> None:
    if not src.is_file():
        raise SystemExit(f"missing Kotlin flavor: {src}")
    kt_stubs = sorted(STUBS_KT.rglob("*.kt"))
    dest = out / label
    dest.mkdir(parents=True, exist_ok=True)
    _run(
        [
            str(kotlinc),
            "-jvm-target",
            "11",
            "-cp",
            str(out),
            "-d",
            str(dest),
            *[str(p) for p in kt_stubs],
            str(src),
        ]
    )
    print(f"OK kotlinc {label}: {src.name}")


def main() -> int:
    if OUT.exists():
        shutil.rmtree(OUT)
    OUT.mkdir(parents=True)
    compile_java_stubs(OUT)
    kotlinc = ensure_kotlinc()
    compile_java_flavor(OUT, JAVA_ROOT / "myket/FastGameStoreActivity.java", "unreal-myket")
    compile_java_flavor(OUT, JAVA_ROOT / "googleplay/FastGameStoreActivity.java", "unreal-googleplay")
    compile_kotlin_bazaar(
        OUT, kotlinc, JAVA_ROOT / "cafebazaar/FastGameStoreActivity.kt", "unreal-cafebazaar"
    )
    print("android store compile OK (unreal: myket + googleplay + cafebazaar)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
