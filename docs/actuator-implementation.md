# Six-channel actuator implementation

Network follow-up: [unified Arduino-ESP32 Network API migration](network-api-migration.md).

Implementation date: 2026-09-27. Based on local `dev` at `81827a2af9410b91741ece1a23cf6ee95877b757`. The earlier production-design documents describe the target architecture; this document records the implemented behavior and remaining qualification work.

## Firmware and dashboard

The `waveshare-relay-6ch` PlatformIO environment adds the actuator to ESP32-SvelteKit. Existing framework services remain: Wi-Fi/AP provisioning, authentication, settings, OTA, NTP and optional MQTT. Other board environments retain the original light-demo entry point and exclude actuator/KNX sources. The actuator does not add a separate MQTT control API.

One FreeRTOS owner task serializes relay operations, button gestures, status indications, protocol processing and queued HTTP mutations. UART receive callbacks only enqueue bounded frames. Relays start LOW before framework initialization. Relay state means commanded GPIO state, not measured contact feedback.

The `/device` dashboard groups Outputs, Indicators, Button, Modbus, KNX and Maintenance. Server-enforced roles are viewer, operator, installer and administrator, with per-user channel masks. Viewer can inspect status, operator can issue authorized channel commands, installer can configure hardware/protocols, and administrator can manage accounts and reset. Installer-only settings and protocol interface write masks are independent of operator channel permissions.

- Six individual outputs: ON/OFF, pulse, startup state, enable, network-loss policy and channel names.
- RGB: enable, brightness and timed dashboard color tests; AP blue, AP with client cyan, active uplink green, disconnected amber, fault flashing red, KNX programming red and identification white. Factory-reset warning has highest priority. Patterns are fixed in firmware.
- Passive buzzer: enable, test, acknowledgement, bounded frequency/duration/duty controls from the dashboard and Modbus, two connection chirps and rate-limited fault/disconnection indication.
- BOOT: debounced single/double/triple clicks, configurable action and channel. Four or more clicks are ignored. Holding arms reset at five seconds and resets at ten seconds; release cancels. Application-click disable does not disable physical recovery. BOOT held at startup is ignored until released (the ROM boot function remains hardware controlled).
- RS485: fixed TX17/RX18, automatic board direction control, 9600–115200 baud and 8E1/8O1/8N2/8N1. RTU requires RS485 enabled; conflicting settings are rejected.
- Exclusive protocol interface selection: off, Modbus RTU, Modbus TCP, or KNXnet/IP. The previous stack stops before the replacement starts. IP protocols wait for an active IPv4 uplink and rebind after address or interface changes.

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

`knx/product-model.json` generates the XML source and firmware constants. `knx/Edge_S3_Relay_6CH.knxprod` was produced by OpenKNXproducer 4.3.5. The producer's structural checks passed; it reported no local XSD, so schema validation and ETS import/download are not established by that build.

## Reproduce checks

For the ETS message **No valid license found to test the unregistered product(s) in this file**, see the [licensing diagnosis and import prerequisites](../knx/README.md). Structural generation success does not establish ETS import eligibility.

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

No board was flashed or bench-tested in this task. Confirm fitted flash size against the 8 MB partition choice, relay startup/strapping behavior, RS485 automatic turnaround and parity/timing, buzzer/RGB operation, power-loss persistence and OTA/reset behavior. Exercise both protocol interface transports with real masters, ETS import/full/partial download, multicast behavior on the target WLAN, group reads/status and concurrent web/ETS ownership changes. The `.knxprod` is not a certification claim.

Management currently inherits HTTP from the framework; deploy only on a controlled management network until the intended transport/security architecture is qualified. Exhaustive HTTP permission tests, physical interruption tests and device-in-loop KNX tests remain release work. The design's proposed granular resource-grant model, configurable indicator patterns, factory fixture automation and signed production identity lifecycle are not implemented here.

## KNX persistence repair (2026-10-07)

The 8,192-byte KNX image was encoded as one 16,384-character hex string, then nested as one JSON payload string in DurableStore. ArduinoJson 7.4.3 grows stream-parser strings geometrically, requiring a block larger than 32 KB for either representation. The failing device reported 79,040 free heap bytes but a largest free block of only 31,732 bytes; filesystem usage was only 65,536 of 1,572,864 bytes. Additional copies of both storage generations and serialization buffers aggravated the allocation pressure.

New KNX images and durable envelopes use arrays of at most 1,024 characters per string. Legacy string formats remain readable. Slot selection retains metadata only; serialization temporaries are released before read-back verification. Allocation failure while inspecting slots aborts the write rather than treating an unreadable generation as disposable. Existing checksums and two-generation recovery remain in place.

Persistence failures now report the failing stage. Background saves retry at five-second intervals rather than every actuator loop, and a successful commit clears fault 5 without clearing unrelated faults. Table-loading failures are distinguished from durable-write failures in the web response.

Native regression coverage exercises a complete 16 KB image with JSON allocations capped at 8 KB, reproduces rejection of the legacy representation under that cap, and verifies successive saves, short-write recovery, open failure, allocation failure without slot modification, restart, checksum corruption, and legacy migration. New chunked envelopes require this firmware or later to read; downgrading firmware after saving is not a supported storage migration.

Hardware validation: the waveshare-relay-6ch build passed and OTA returned HTTP 200. The board accepted the requested switch addresses 8/0/0–8/0/5 and status addresses 8/1/0–8/1/5 with HTTP 200. All 12 mappings were read back after a software restart; KNX was configured/running and fault was 0. Firmware SHA-256: `127d12f7f99351582654460d8ef8a44bd559cf01862f82887d223a57890a6906`. Local evidence is in `.pio/knx-persistence-validation/` and the build log is `.pio/knx-persistence-build.log`. This validates persistence, not physical relay actuation or an ETS bus interoperability test.

## KNX individual-address commissioning

The KNX form submits the individual address, all object mappings and all application parameters as one commissioning transaction. The device object is the canonical runtime address used by incoming-address filtering, outgoing telegram source addresses, and KNXnet/IP search/description responses. Its save/restore data is part of the same durable image; failed commits restore the previous image.

The commissioning response now returns the committed KNX snapshot and its own revision, rather than the unrelated general-settings revision. The page consumes that snapshot and prevents edits/reloads during an in-flight save. Firmware, form and simulator share the same address limits and notation; firmware rejects whitespace, signs, trailing characters and oversized components before mutation. Leading zeros normalize on read-back. A restored web-owned image now retains its owner after startup instead of being misclassified as a new ETS download.

Regression coverage includes non-default and boundary individual addresses, invalid input, combined address/group/parameter saves, address-only edits preserving other settings, failed-save rollback, and ownership/address persistence after restart.

Hardware verification: OTA succeeded; an address-only commissioning request changed the individual address to **1.1.10** and returned the committed KNX snapshot/revision. All group mappings and application parameters matched the pre-save snapshot. After a software restart, the address remained 1.1.10, owner remained web, and fault was 0. A UDP KNXnet/IP SearchResponse also advertised 1.1.10. The routing-only build does not enable the DescriptionRequest handler (it is conditional on KNX_TUNNELING upstream), so discovery verification used the supported SearchRequest service. Validation evidence: `.pio/knx-address-validation/`; build log: `.pio/knx-address-build.log`. Firmware SHA-256: `d054b7ae238b5e021f2030ad29eb53944b09874a8781bb68c349b30a0a57e34e`.

Validation passed: native regressions, 22 simulator tests, the Chromium KNX commissioning test, UI type checking (zero errors/warnings), and the waveshare-relay-6ch firmware build. Actual relay telegram actuation was not part of this address-only verification.
