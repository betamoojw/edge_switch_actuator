# Hardware-independent frontend development and testing

The real Svelte frontend can run against a local stateful simulator or real
firmware. Simulator code is under `interface/simulator/`; the UI has no simulator
branches or mocked API client. Production build and firmware packaging commands
are unchanged. Use Node 24 and npm from `interface/`.

## Quick start

```sh
npm ci
npm run dev:sim
```

Open the localhost URL printed by Vite. The default profile models the six-relay
actuator. Sign in with `admin` / `sim-admin`. Other local-only fixture users are
`installer` / `sim-installer`, `operator` / `sim-operator` (channel 1 only), and
`viewer` / `sim-viewer` (read-only). These are simulator credentials, not device
setup passwords. No external device, MQTT broker or KNX network is contacted.

The actuator profile also models **Connections → Xiaozhi MCP** without contacting
any external MCP service. The `mcp-only` and `mcp-off` profiles cover feature-aware
navigation. Send `{ "type": "mcp", "state": "ready" }` to the authenticated
`/__sim/actions` control endpoint to select a deterministic connection state.
See [Xiaozhi MCP](xiaozhi-mcp.md) for settings, tools, native tests and the optional
local WSS peer. Browser cases `MCP-*` exercise configuration and access roles.

For a real device, set DEVICE_HOST explicitly:

```powershell
$env:DEVICE_HOST='192.168.1.111'
npm run dev:device
```

```sh
DEVICE_HOST=192.168.1.111 npm run dev:device
```

Both modes proxy `/rest` and `/ws` unchanged. `npm run dev` and `npm run preview`
also require DEVICE_HOST. A bare host, host:port, or HTTP(S) origin is accepted;
credentials, paths and query strings are rejected. `npm run build` requires no
target. The `.env.example` documents shell variables; the runner does not load
that example file automatically.

## Marketing demo video

The Playwright recorder produces a 1280×720 dark-theme demonstration from the
real actuator interface backed by the local simulator. It signs in before
recording, tours Outputs, Indicators, Button, Protocol, KNX and Maintenance,
returns all outputs to OFF, and marks the result as a simulator demonstration.
The implementation brief is saved in
[the marketing demo agent prompt](tasks/device-marketing-demo-video-agent-prompt.md).

Install dependencies and the Chromium runtime once:

```sh
cd interface
npm ci
npx playwright install chromium
```

Start the simulator in one terminal and leave it running:

```sh
cd interface
npm run dev:sim
```

After Vite reports `http://127.0.0.1:5173`, record from a second terminal:

```sh
cd interface
npm run video:marketing
```

The primary artifact is
`interface/test-results/marketing/edge-switching-actuator-demo.webm`. The script
also attempts to create an MP4 in the same directory. Playwright's bundled
FFmpeg may not include an H.264 encoder; in that case the WebM remains the
supported output and the command reports that MP4 conversion is unavailable.

Use `DEMO_BASE_URL` when the simulator frontend is on another local port,
`DEMO_OUTPUT_DIR` to select another output directory, and `DEMO_OUTPUT_NAME` to
set the filename without a path or extension:

```powershell
$env:DEMO_BASE_URL='http://127.0.0.1:5174'
$env:DEMO_OUTPUT_DIR='test-results/marketing-candidate'
$env:DEMO_OUTPUT_NAME='actuator-dark-tour'
npm run video:marketing
```

```sh
DEMO_BASE_URL=http://127.0.0.1:5174 \
DEMO_OUTPUT_DIR=test-results/marketing-candidate \
DEMO_OUTPUT_NAME=actuator-dark-tour \
npm run video:marketing
```

Review the opening title, relay interactions, all six section views and closing
title before publishing. Confirm the output is 1280×720, contains changing
frames, uses the dark theme, and retains the **Simulator demo** disclosure. The
recorder uses only local fixture credentials, does not save configuration
changes and finishes with all relay outputs OFF. Set `SIM_CONTROL_TOKEN` and
optionally `SIM_CONTROL_URL` for simulator-originated button gestures. Stop the
simulator with Ctrl+C after recording.

## Profiles, persistence and lifecycle

Set SIM_PROFILE to `actuator` (default), `knx` (ETS-owned commissioned fixture),
`template` (demo LED/broker settings and sleep), `ethernet`, `battery`, `json`
(JSON events instead of default binary MessagePack), or `security-off`.

Profiles are separate firmware capabilities, not a superset pretending every
endpoint exists. The current UI always links to the actuator and the home page
redirects to `/device`; the demo remains an explicit template-only route; an actuator profile does not invent lightState/brokerSettings
handlers. Unregistered REST paths return a small HTML fallback sentinel, matching
the device's non-JSON SPA fallback behavior but not its complete HTML bytes.
Ethernet/battery/security-off are framework coverage fixtures, not claims that
the shipped relay board has those capabilities.

Default state is memory-only. Set SIM_STATE_FILE to `.sim-state/actuator.json` to
retain settings across process restarts. State is written atomically; fixture
passwords are stored as salted scrypt hashes. A state file cannot be loaded under
a different profile. Reboot retains settings and commissioning, clears runtime
state, applies startup outputs, closes sockets and invalidates tokens. Factory
reset restores simulator fixture settings and personas, unlike a physical
actuator's device-specific provisioning identity. Stop the runner with Ctrl+C.

Optional ports: FRONTEND_PORT (5173), SIM_PORT (3080), SIM_CONTROL_PORT (3081).
All simulator listeners bind 127.0.0.1. Port collisions fail startup. Run only
one suite per port set; CI uses one worker per independently isolated job.

## Control API and reproducible scenarios

The control API is on a separate loopback port, never proxied through the
production `/rest` or `/ws` routes. GET `/__sim/health` is a public readiness
check. Every other control operation requires `Authorization: Bearer <token>`;
browser Origin requests are rejected. Set SIM_CONTROL_TOKEN or use the random
token printed by dev:sim. Test runs use their own deterministic local token.

Example PowerShell (substitute the printed token):

```powershell
$simHeaders = @{Authorization='Bearer YOUR_LOCAL_CONTROL_TOKEN'}
Invoke-RestMethod http://127.0.0.1:3081/__sim/actions -Method Post `
  -Headers $simHeaders -ContentType application/json `
  -Body '{"type":"relay","channel":0,"value":true,"source":"external"}'
```

| Path / method         | Example body or result                                                                                                            |
| --------------------- | --------------------------------------------------------------------------------------------------------------------------------- |
| GET /\_\_sim/state    | Redacted status/config/KNX snapshot, connection count and OTA state; no users/passwords/tokens                                    |
| POST /\_\_sim/reset   | `{ "profile": "actuator" }`: reset fixture, timers, sessions, sockets and injected faults                                         |
| POST /\_\_sim/clock   | `{ "advanceMs": 61000 }`: advance device timers, maximum one day per call                                                         |
| POST /\_\_sim/actions | `{ "type":"relay", "channel":0, "value":true }`                                                                                   |
| POST /\_\_sim/actions | `{ "type":"network", "wifi":false, "ap":true }`: AP alone is not an uplink                                                        |
| POST /\_\_sim/actions | `{ "type":"knx-object", "number":1, "value":true }`: decoded Switch object; object 2 Block, object 3 Status                       |
| POST /\_\_sim/actions | `{ "type":"knx", "busy":true, "owner":"ets" }`: commissioning interlock/ownership                                                 |
| POST /\_\_sim/actions | `{ "type":"modbus", "channel":0, "value":true, "clients":1 }`: decoded bus effect/counters                                        |
| POST /\_\_sim/actions | `{ "type":"gesture", "clicks":1 }`, or pressed/resetArmed/holdMs for recovery state                                               |
| POST /\_\_sim/actions | `{ "type":"offline", "active":true, "durationMs":5000 }`: API unavailable while controls remain accessible                        |
| POST /\_\_sim/actions | `{ "type":"failure", "target":"persistence" }` or protocol: next configuration failure                                            |
| POST /\_\_sim/actions | `{ "type":"reboot" }`, `{ "type":"factory-reset" }`                                                                               |
| POST /\_\_sim/actions | `{ "type":"ota", "outcome":"Simulated write failure" }`, or success                                                               |
| POST /\_\_sim/actions | scan/networks array; mqtt/connected/error; battery/soc/charging; coredump/available; fault/code/error; notification/level/message |
| POST /\_\_sim/faults  | `{ "path":"/rest/device/status", "effect":"http", "status":503, "count":2 }`                                                      |
| POST /\_\_sim/faults  | effect latency with ms; malformed JSON; disconnect; hold; optional method and count (default 1)                                   |
| POST /\_\_sim/faults  | `{ "clear":true }`: clears rules, releases held replies as 503, restores event delivery                                           |
| POST /\_\_sim/socket  | action close, malformed, pause, refuse; pause/refuse accept active:false to restore                                               |

Use production config/command routes to change protocol and settings; control
actions represent device-originated stimuli. KNX and Modbus require the selected
running protocol. Relay commands respect enabled/block state. The simulator
models independent actuator/KNX revisions, explicit ETS takeover, user channel
masks, deduplicated commands, pulses, network-loss/watchdog off policy, indicator
brightness and command durations. Settings responses retain firmware envelopes;
authorization distinguishes framework 401 from actuator 403. Bodyless responses,
scan 202s, binary coredumps and multipart uploads are covered by contract tests.

## Automated checks

```sh
npm run check
npm run build
npm run test:bundle
npm run test:sim
npx playwright install chromium firefox webkit
npm run test:e2e
npm run test:e2e:built
```

`test:sim` runs Node's test runner, including real HTTP/WebSocket requests and
model tests. `test:e2e` starts a fresh simulator and Vite, runs the real UI with
Chromium, Firefox, WebKit and a mobile Chromium viewport, and stops services.
Use `npm run test:e2e -- --project=chromium` for a shorter loop. Do not run an
unrelated Vite server on the configured test ports. The built suite uses
adapter-static output and preview proxies; build first. Do not rebuild or edit
frontend files while a browser suite is running, since Vite reloads the page.

Playwright intercepts only external GitHub release lookup, so tests do not depend
on GitHub availability or downloads. Device REST and socket traffic is real.
Browser contexts and device fixtures reset between tests; use response/DOM
assertions and bounded polling, not networkidle. Reports, JUnit results, traces,
screenshots and videos are under ignored `playwright-report/` and `test-results/`.
Built and development reports have separate paths. `test:bundle` checks for
simulator/control/test markers in production assets.

If a corporate proxy intercepts localhost, ensure the test process can access
127.0.0.1 directly. The local Windows validation run needed proxy variables
cleared in its shell; this is not applied globally or by the application.

CI: `.github/workflows/frontend-tests.yml` runs check/build/bundle/contract checks
and isolated browser jobs on dev/main pushes and relevant PRs, uploads artifacts
on failures, and runs the built frontend suite in the Chromium job. The existing
MkDocs deployment workflow is unchanged.

## Coverage and physical-device verification

The startup and address-entry changes add `HOME-01` and `KNX-04`: opening `/`
starts Switching Actuator, and invalid individual/group address entries disable
commissioning Apply. See [KNX address entry](knx-address-entry.md) for notation,
placeholders and firmware limits. The browser suite now contains 39 scenarios
per project; the 37-test results below describe the earlier implementation run.

Browser tests are named with matrix IDs from
[the design plan](FRONTEND_HARDWARE_INDEPENDENT_TEST_PLAN.md). They cover login,
permissions, relay and externally originated state, profile save/discard/reboot,
errors and held requests, RGB/tone/button, RTU/TCP/KNX settings and commissioning,
Wi-Fi/AP/Ethernet, MQTT/NTP/users, OTA, telemetry/navigation, coredump, reset and
template LED. Transport contract tests add unauthorized requests, profile absence,
rollback, stale revisions, duplicate commands, scan polling and both codecs.
Assertions cover representative behavior and boundaries; the plan's full matrix
also includes physical and more exhaustive field combinations that are not all
automatable against a simulator.

To run the read-only physical actuator smoke test:

```powershell
$env:DEVICE_HOST='192.168.1.111'
$env:DEVICE_USERNAME='admin'
$env:DEVICE_PASSWORD='YOUR_DEVICE_SETUP_PASSWORD'
npm run test:e2e:device -- --project=chromium
```

E2E_BASE_URL can instead select the device-hosted SPA. The device suite performs
login and reads only; it never calls control APIs, changes relays/settings, or
resets/flashes hardware. Use `npm run contract:capture -- test-results/device.json`
to capture sanitized read-only response shapes for manual comparison with a
matching simulator profile. See `interface/tests/contracts/README.md` for source
authority and comparison requirements. Real-device/HIL results remain outstanding
until an actual device and test rig are provided. Follow section 11 of the design
plan for physical contacts, RS485 timing, ETS interoperability, power loss and OTA
qualification; a simulator pass cannot substitute for those checks.

## Deliberate simulation limits and existing UI behavior

- Measurements, scan results, MAC/IP identities, coredump bytes and NTP local time
  are synthetic. Timezone configuration round-trips, but no POSIX timezone engine
  or real NTP synchronization runs. No physical contact sensing is claimed.
- KNX/Modbus injection models decoded application effects, not their wire stacks,
  ETS downloading or TCP peer authentication. Profile fixtures do not emulate RF,
  UART, PHY or flash. Queue-full errors can be injected; FreeRTOS scheduling and
  byte-for-byte ArduinoJson serialization are not emulated.
- Firmware images are accepted only as bounded multipart fixtures with an ESP32-S3
  header; checksum/error and progress UI are simulated. No image is flashed and
  download_url is never fetched. MD5 chunk-boundary quirks and partition sizing
  need real-firmware comparison. Binaries are kept in memory only during upload.
- Framework defaults/normalization follow source where implemented; hardware
  library coercion details and device-specific provisioning still require captured
  fidelity checks. The Wi-Fi updater's current double counter increment is retained.
- Manual indicator commands/timers and colors are modeled; autonomous connection
  chirps and exact RTOS blink/tone scheduling are not physical timing guarantees.
- The current frontend has no fetch deadline: a held command remains busy until
  the transport resolves. Tests explicitly release faults instead of inventing
  timeout recovery. Socket reconnect and listener cleanup limitations described
  in the plan remain production behavior. Session invalidation redirects to home.
- A dashboard is optional and not included; the authenticated control API provides
  all automated scenario controls without adding UI mocks or firmware routes.

## Implementation validation (2026-09-28)

Local Windows validation completed with Node 24:

| Check                                           | Result                                                                                             |
| ----------------------------------------------- | -------------------------------------------------------------------------------------------------- |
| Simulator contract suite                        | 12 passed                                                                                          |
| Chromium / WebKit / mobile Chromium             | 37 each; 111 passed                                                                                |
| Production static build, Chromium               | 37 passed on full rerun                                                                            |
| Svelte / TypeScript                             | 0 errors, 0 warnings                                                                               |
| Production build and simulator marker exclusion | Passed; existing large-chunk warning remains                                                       |
| Existing Python product contracts               | 3 passed                                                                                           |
| Formatting of added code and Vite config        | Passed                                                                                             |
| Firefox                                         | Blocked before page launch by Windows SideBySide/mozglue assembly error, including after reinstall |

The first built-suite attempt lost its local server during login; an isolated
login retry and the subsequent complete 37-test run passed. Reports remain under
the ignored test output directories. Firefox stays enabled in CI and locally;
its failures are not skipped or counted as application passes. CI execution and
physical-device/HIL verification have not been performed in this workspace.
