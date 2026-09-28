"""Exercise the production MQTT adapter with fake transport and real ArduinoJson."""

import os
import subprocess

from test_native import ROOT, find_compiler


def main() -> None:
    headers = next((ROOT / ".pio/libdeps").glob("*/ArduinoJson/src/ArduinoJson.h"), None)
    if headers is None:
        raise SystemExit("Install PlatformIO project dependencies before running the Home Assistant adapter tests")
    binary = ROOT / ".pio" / ("home-assistant-tests.exe" if os.name == "nt" else "home-assistant-tests")
    subprocess.run(
        find_compiler()
        + ["-std=c++17", "-Itests/ha_stubs", "-Isrc", "-I" + str(headers.parent), "tests/home_assistant_tests.cpp", "-o", str(binary)],
        cwd=ROOT,
        check=True,
    )
    subprocess.run([str(binary)], cwd=ROOT, check=True)


if __name__ == "__main__":
    main()
