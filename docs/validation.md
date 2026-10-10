# Validation record

## Documentation refresh — 10 October 2026

Reviewed firmware source: `dev` at `ced3e6d`, version 0.6.3. This refresh changes
documentation and its publishing workflow; it does not modify firmware or
frontend behavior. Tests below run against the existing source checkout.

| Check | Result in this refresh |
| --- | --- |
| Portable gesture and Modbus PDU tests | Passed |
| Selected network uplink tests | Passed |
| DurableStore large-image/recovery tests | Passed |
| KNX product contracts invoked by native runner | 6 passed |
| Home Assistant native adapter tests | Passed |
| MCP protocol/settings/relay policy tests | Passed |
| MCP transport and lifecycle tests, compile guards | Passed |
| Frontend simulator contracts | 25 passed |
| Translation checks | 2 passed |
| Svelte/TypeScript diagnostics | 0 errors, 0 warnings |
| Python credential/QR-label and release-packaging tests | 28 passed after installing declared dependencies |
| MkDocs production build | Passed with `--strict`, including internal links and anchors |
| Rendered documentation | Chromium search passed; desktop, architecture diagram and mobile quick start visually inspected; no mobile page overflow |

The first sandboxed run could not start Zig and denied some localhost/file
operations; these checks passed when rerun with the required host access. Python
credential-label tests initially lacked their optional image dependencies and
passed after installation. No physical device was connected, flashed, reset
or operated during this documentation task.

## Reproduce software checks

Install the tools in [getting started](gettingstarted.md). Portable C++ tests
need `CXX`, clang++, g++ or the locally supported Zig installation. Native storage
and MCP tests also need PlatformIO's ArduinoJson headers installed under
`.pio/libdeps/`.

```sh
python scripts/test_native.py
python scripts/test_home_assistant.py
python scripts/test_xiaozhi_mcp.py
python -m pip install -r scripts/requirements-credentials.txt zxing-cpp==2.3.0
python -m unittest discover -s tests -p "test_*.py"
cd interface
npm run test:sim
npm run test:i18n
npm run check
npm run build
npm run test:bundle
npx playwright install chromium firefox webkit
npm run test:e2e
npm run test:e2e:built
```

Browser suites and a fresh firmware build are separate checks; commands listed
here are not claims that each was run during this documentation refresh. The
firmware matrix, browser matrix and credential tests live in the corresponding
files under `.github/workflows/`.

## Existing evidence and its limits

GitHub CI was also inspected for the exact reviewed commit `ced3e6d`:

| Existing run | Observed result |
| --- | --- |
| [Frontend functional tests](https://github.com/betamoojw/edge_switch_actuator/actions/runs/38049531978) | Successful |
| [Firmware and native matrix](https://github.com/betamoojw/edge_switch_actuator/actions/runs/38049531980) | Seven profiles passed, including the Waveshare actuator and both MCP variants; ESP32-C3 failed |

The [C3 job](https://github.com/betamoojw/edge_switch_actuator/actions/runs/38049531980/job/114205685278)
reached the size check: **2,039,455 bytes** exceeded the configured **1,966,080-byte**
maximum by **73,375 bytes**. This is a confirmed image-size failure, not the older
local toolchain-download limitation. It needs a separate firmware/profile change;
the documentation refresh does not change partitions or remove firmware features.

Publication of the initial refresh at `5642042` passed both jobs in the
[Documentation run](https://github.com/betamoojw/edge_switch_actuator/actions/runs/38057005002).
The public home and REST API pages returned HTTP 200 with the refreshed content.

- [Implementation history](actuator-implementation.md) records earlier software
  checks, KNX persistence/address hardware checks and their artifact hashes.
- [MCP hardware validation](tasks/xiaozhi-mcp-hardware-validation.md) and
  [HTTP stack repair](tasks/xiaozhi-mcp-stack-overflow-fix.md) record narrower
  board checks on their stated firmware, not a qualification of every later build.
- [Modbus TCP acceptance](modbus-tcp-relay-fat-sat.md) and
  [Modbus RTU acceptance](modbus-rtu-relay-fat-sat.md) are procedures. Their presence
  alone is not evidence of a pass.
- The [KNX package record](https://github.com/betamoojw/edge_switch_actuator/blob/dev/knx/README.md)
  distinguishes structural/registration-metadata checks from ETS import and
  actual device download. Successful ETS acceptance is not established there.
- Local `.pio/` evidence paths named in old records are not served by this site.
  An older build manifest is not a fresh artifact identity for 0.6.3.

## Remaining release work

Validate real relay/indicator behavior, startup pins and fitted flash, RS485
turnaround and timing, power interruption during saves/reset, and each intended
master/ETS workflow. Exercise invalid TLS certificates and hostnames, reconnect
and OTA lifecycle under load, heap/stack limits and prolonged operation. Confirm
firmware/image compatibility and inspect [current source findings](actuator-source-review.md)
before relying on the GitHub release picker.
