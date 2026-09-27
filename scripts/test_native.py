"""Run portable firmware regression tests with CXX, clang++, g++, or local Zig."""

import os
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def find_compiler() -> list[str]:
    if os.environ.get("CXX"):
        return shlex.split(os.environ["CXX"])
    zig = ROOT / ".pio/native-tools/ziglang/zig.exe"
    if zig.exists():
        return [str(zig), "c++"]
    return [shutil.which("clang++") or shutil.which("g++") or "c++"]


def main() -> None:
    cases = [
        ("native-tests", "tests/native_tests.cpp", ["-Isrc"]),
        ("network-tests", "tests/network_tests.cpp", ["-Itests/network_stubs", "-Ilib/framework"]),
    ]
    for name, source, includes in cases:
        binary = ROOT / ".pio" / (name + (".exe" if os.name == "nt" else ""))
        binary.parent.mkdir(exist_ok=True)
        subprocess.run(find_compiler() + ["-std=c++17", *includes, source, "-o", str(binary)], cwd=ROOT, check=True)
        subprocess.run([str(binary)], cwd=ROOT, check=True)
    subprocess.run([sys.executable, "tests/product_contract_test.py"], cwd=ROOT, check=True)


if __name__ == "__main__":
    main()
