"""Test the production MCP protocol, settings and relay policy without hardware."""
import os
import subprocess
from test_native import ROOT, find_compiler

headers = next((ROOT / ".pio/libdeps").glob("*/ArduinoJson/src/ArduinoJson.h"), None)
if headers is None:
    raise SystemExit("Install PlatformIO dependencies first")
binary = ROOT / ".pio" / ("xiaozhi-mcp-tests.exe" if os.name == "nt" else "xiaozhi-mcp-tests")
subprocess.run(find_compiler() + ["-std=c++17", "-Isrc", "-Ilib/framework", "-I" + str(headers.parent),
    "tests/xiaozhi_mcp_tests.cpp", "lib/framework/XiaozhiMcpProtocol.cpp", "-o", str(binary)], cwd=ROOT, check=True)
subprocess.run([str(binary)], cwd=ROOT, check=True)
transport = ROOT / ".pio" / ("xiaozhi-mcp-transport-tests.exe" if os.name == "nt" else "xiaozhi-mcp-transport-tests")
subprocess.run(find_compiler() + ["-std=c++17", "-Itests/mcp_stubs", "-Ilib/framework", "-I" + str(headers.parent),
    "tests/xiaozhi_mcp_transport_tests.cpp", "lib/framework/XiaozhiMcpTransport.cpp", "lib/framework/XiaozhiMcpProtocol.cpp",
    "-o", str(transport)], cwd=ROOT, check=True)
subprocess.run([str(transport)], cwd=ROOT, check=True)
life = ROOT / ".pio" / ("xiaozhi-mcp-lifecycle-tests.exe" if os.name == "nt" else "xiaozhi-mcp-lifecycle-tests")
subprocess.run(find_compiler() + ["-std=c++17", "-Itests/mcp_stubs", "-Ilib/framework",
    "tests/xiaozhi_mcp_lifecycle_tests.cpp", "-o", str(life)], cwd=ROOT, check=True)
subprocess.run([str(life)], cwd=ROOT, check=True)

# Compile guards must fail closed without security/time or with public config files.
for flags, valid in [([], True), (["-DFT_XIAOZHI_MCP=1", "-DFT_MQTT=0"], True),
                     (["-DFT_XIAOZHI_MCP=1", "-DFT_SECURITY=0"], False),
                     (["-DFT_XIAOZHI_MCP=1", "-DFT_NTP=0"], False),
                     (["-DFT_XIAOZHI_MCP=1", "-DSERVE_CONFIG_FILES=1"], False)]:
    checked = subprocess.run(find_compiler() + ["-x", "c++", "-E", "-Ilib/framework", *flags, "-"],
                             input='#include "Features.h"\n', text=True, capture_output=True, cwd=ROOT)
    assert (checked.returncode == 0) == valid, checked.stderr
print("MCP feature configuration guards passed")
