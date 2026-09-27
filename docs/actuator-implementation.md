# Six-channel actuator implementation

Network follow-up: [unified Arduino-ESP32 Network API migration](network-api-migration.md).

Implementation date: 2026-09-27. Based on local `dev` at `81827a2af9410b91741ece1a23cf6ee95877b757`. The earlier production-design documents describe the target architecture; this document records the implemented behavior and remaining qualification work.

## Firmware and dashboard

The `waveshare-relay-6ch` PlatformIO environment adds the actuator to ESP32-SvelteKit. Existing framework services remain: Wi-Fi/AP provisioning, authentication, settings, OTA, NTP and optional MQTT. Other board environments retain the original light-demo entry point and exclude actuator/KNX sources. The actuator does not add a separate MQTT control API.

One FreeRTOS owner task serializes relay operations, button gestures, status indications, protocol processing and queued HTTP mutations. UART receive callbacks only enqueue bounded frames. Relays start LOW before framework initialization. Relay state means commanded GPIO state, not measured contact feedback.

The `/device` dashboard groups Outputs, Indicators, Button, Modbus, KNX and Maintenance. Server-enforced roles are viewer, operator, installer and administrator, with per-user channel masks. Viewer can inspect status, operator can issue authorized channel commands, installer can configure hardware/protocols, and administrator can manage accounts and reset. Installer-only settings and fieldbus write masks are independent of operator channel permissions.

- Six individual outputs: ON/OFF, pulse, startup state, enable, network-loss policy and channel names.
- RGB: enable, brightness and timed dashboard color tests; AP blue, AP with client cyan, active uplink green, disconnected amber, fault flashing red, KNX programming red and identification white. Factory-reset warning has highest priority. Patterns are fixed in firmware.
- Passive buzzer: enable, test, acknowledgement, bounded frequency/duration/duty controls from the dashboard and Modbus, two connection chirps and rate-limited fault/disconnection indication.
- BOOT: debounced single/double/triple clicks, configurable action and channel. Four or more clicks are ignored. Holding arms reset at five seconds and resets at ten seconds; release cancels. Application-click disable does not disable physical recovery. BOOT held at startup is ignored until released (the ROM boot function remains hardware controlled).
- RS485: fixed TX17/RX18, automatic board direction control, 9600–115200 baud and 8E1/8O1/8N2/8N1. RTU requires RS485 enabled; conflicting settings are rejected.
- Exclusive fieldbus selection: off, Modbus RTU, Modbus TCP, or KNXnet/IP. The previous stack stops before the replacement starts. IP protocols wait for an active IPv4 uplink and rebind after address or interface changes.

## API and persistence

Authenticated GET endpoints: `/rest/device/status`, `/rest/device/config`, `/rest/knx/config`. Mutations use POST to `/rest/device/commands`, `/rest/device/config`, `/rest/protocol/transition`, `/rest/knx/config`, `/rest/knx/programming`. Configuration writes require the current revision. Commands accept an optional `requestId` (maximum 64 characters); the last 16 responses per device are deduplicated by user and ID for 60 seconds. The dashboard supplies IDs. Reusing an ID with a different payload is rejected. IDs are not durable across restart. Device WebSocket events are read-only; the dashboard also polls every two seconds.

Actuator settings and the complete KNX image use alternating generation/checksum files, with read-back verification. Incomplete ETS images are not committed over the last complete image. The framework's pre-existing generic settings persistence remains separate. Factory reset drives outputs off and uses a resumable marker before deleting `/config`; manufacturing identity in NVS remains.

The Waveshare profile creates a random 24-character setup password in NVS, used for the factory AP and initial `admin` account. Read it from the local 115200-baud serial console during bring-up and retain it in the unit's manufacturing record. There is no default guest on this profile. Existing accounts migrate without replacement. Passwords are salted PBKDF2 hashes; settings responses omit stored password material and signing secrets. Login tokens expire after eight hours and restart; administrative account edits rotate the signing secret. First boot formats only an entirely erased filesystem partition. A corrupt existing filesystem is preserved for recovery.

## Modbus

See [register map](actuator-modbus-map.md) for zero-based addresses. Implemented functions: 01, 02, 03, 04, 05, 06, 0F, 10, serial 08/0000 echo, and 2B/0E device identification. Relay writes follow standard coil values. Configuration writes are staged behind an installer-opened window, require revision consistency, and use the command mailbox to commit. Broadcast only permits relay coil writes and never replies. Mailbox results have bounded peer/sequence deduplication.

TCP uses MBAP framing, four bounded client slots, fragmented/coalesced request handling, optional exact IPv4 source restriction and idle timeout. A configuration window opened for an IP binds to its first requesting TCP session. RTU uses UART hardware idle events, CRC checks and bounded frame queues. Exact 1.5-character gap rejection within a UART-buffered frame is not implemented; verify strict RTU timing requirements against the intended masters before release. There is no Modbus authentication or transport encryption.

## KNX

Uses pinned `thelsing/knx` commit `980c047ad7fc5e27bf2fae95e48acde5d5e0b4fd`, mask 57B0 and Wi-Fi IP transport. Each channel has Switch (DPT 1.001), Block (DPT 1.003) and Status (DPT 1.002): 18 objects total, with up to 64 unique group addresses and 96 associations. Firmware accepts the matching manufacturer/application/version and 52-byte parameter layout. Invalid application parameters prevent actuator callbacks.

Button, dashboard and ETS share the stack's programming state. Programming expires after five minutes when a download is not busy. Web commissioning edits the actual stack tables and persists the same image used by ETS; it requires explicit takeover from ETS ownership. Commissioned relay parameters must be edited in the KNX panel, not the general Outputs panel. Group-address order determines the first transmit association.

`knx/product-model.json` generates the XML source and firmware constants. `knx/Edge_S3_Relay_6CH.knxprod` is a real packaged development product produced by OpenKNXproducer 4.3.5. Manufacturer 0x00FA/application 600 is a development identity, not a registered commercial product. The producer's structural checks passed; it reported no local XSD, so schema validation and ETS import/download are not established by that build.

## Reproduce checks

```text
python scripts/generate_knx_product.py
OpenKNXproducer create knx/Edge_S3_Relay_6CH.xml
python scripts/test_native.py
cd interface
npm ci
npm run build
npm run check
cd ..
platformio run -e waveshare-relay-6ch
```

The native runner uses `CXX`, a local Zig installation, clang++ or g++. Native tests exercise actual portable gesture and Modbus PDU code, including malformed/truncated requests, response bounds, exceptions, broadcast restrictions, click grouping, debounce, hold/reset and uptime wrap. Python contract tests inspect generated XML and packaged application identity. They do not emulate the hardware or KNX stack.

Validation on this machine: native tests and three product-contract tests passed; dashboard production build passed. The subsequent frontend cleanup resolved the remaining 58 type errors and three CSS diagnostics: Svelte checking now reports zero errors and zero warnings. The `svelte-focus-trap` metadata warning is also resolved by the documented local packaging fix in `interface/vendor/svelte-focus-trap/README.md`. Firmware build results and artifact hashes in `actuator-build-manifest.json` describe the Network API migration build, before this frontend cleanup.

## Required qualification before release

No board was flashed or bench-tested in this task. Confirm fitted flash size against the 8 MB partition choice, relay startup/strapping behavior, RS485 automatic turnaround and parity/timing, buzzer/RGB operation, power-loss persistence and OTA/reset behavior. Exercise both fieldbus transports with real masters, ETS import/full/partial download, multicast behavior on the target WLAN, group reads/status and concurrent web/ETS ownership changes. The `.knxprod` is not a certification claim.

Management currently inherits HTTP from the framework; deploy only on a controlled management network until the intended transport/security architecture is qualified. Exhaustive HTTP permission tests, physical interruption tests and device-in-loop KNX tests remain release work. The design's proposed granular resource-grant model, configurable indicator patterns, factory fixture automation and signed production identity lifecycle are not implemented here.
