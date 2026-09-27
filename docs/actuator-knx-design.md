# KNXnet/IP and ETS product design

Implementation follow-up: [implemented behavior, build steps and qualification status](actuator-implementation.md).
Status: proposed six-channel product contract. No KNX firmware or ETS-importable `.knxprod` was generated or validated in this review. The product file is a required release deliverable, not satisfied by renaming XML to `.knxprod`.

## Reference conclusions

Use [thelsing/knx](https://github.com/thelsing/knx) as requested, pinned to a qualified commit. Its upstream overview lists ESP32 and ETS configuration support and a GPL-3.0 license. Record licensing/distribution obligations with the release source and notices.

The [Sonoff reference](https://github.com/betamoojw/KNX_Sonoff_4Ch_Pro_Rev2) targets ESP8266, uses separate Wi-Fi provisioning, and describes limited ETS operations. Reuse the actuator concept, not its board configuration or Wi-Fi manager. The existing SvelteKit framework remains responsible for networking.

Its [product XML](https://github.com/betamoojw/KNX_Sonoff_4Ch_Pro_Rev2/blob/master/src/M-Tech_KNX_IP_Actuator_4-Ch.xml) defines switch/block/status triplets, mask MV-57B0 and an empty parameter section. This product needs two more channel triplets and real parameters. Do not reuse its manufacturer/application identity as this product's release identity.

The reviewed [stack facade](https://github.com/thelsing/knx/blob/master/src/knx_facade.h) exposes programming mode, programming LED callbacks, memory read/write, individual-address read and periodic loop operations. It does not establish a complete high-level web-editing API for group-address tables. Implement and test an adapter against the pinned stack instead of inventing setter methods.

## Network and stack lifecycle

Target a KNX IP end device using the stack's qualified IP/System-B configuration; start with the MV-57B0 reference as a compatibility candidate and verify the selected stack's requirements. This is not a TP/IP router or a promise to provide a general-purpose tunneling server. Routing multicast is the baseline design; qualify ETS discovery/management and download using the actual stack and network topology.

Baseline configuration: KNXnet/IP UDP port 3671 and routing multicast 224.0.23.12, on the STA interface. Confirm multicast reception, membership renewal and required management services against the pinned stack. Wi-Fi client isolation and multicast filtering must be disabled/configured appropriately in the installation. No automatic fallback to Modbus on KNX failure.

The protocol task is the only caller of stack APIs and services the loop regularly even when the application is uncommissioned. Network reconnect recreates needed bindings/membership. Leaving KNX clears programming mode, stops traffic and releases stack resources before another bus starts. If the library cannot safely tear down/reinitialize, implement a controlled reboot transition with a pending target and rollback marker; the UI must report reboot required. Do not claim hot switching until teardown is proven.

## One programming state

`KnxProgrammingState` is a view of the stack's actual mode, containing `active`, `changedBy`, `changedAt`, `deadline`, `commissioningBusy` and revision. Button triple-click and authorized web requests enqueue the same `setProgrammingMode` operation. ETS-originated changes are observed from the stack and update the same snapshot. The UI and LED never retain independent programming booleans.

Disable stack ownership of the raw button and LED pins and route its programming indication through the application indicator service. GPIO38 requires WS2812 signaling, not the stack's generic LED pin toggling. The BOOT gesture engine alone owns GPIO0; long hold is always reset. Red steady indicates programming when RGB is enabled and no higher-priority condition masks it.

Default programming timeout is 5 minutes, deferred while an actual download is in progress; clear on successful completion, explicit exit, mode change or reset. Lost browser sessions do not abort an in-flight ETS download. If button actions or RGB are disabled, the web control/status remains available. Reject programming entry outside selected KNX mode; return a clear network-unavailable state if commissioning cannot proceed.

## Six-channel communication objects

Proposed public object numbers deliberately preserve the reference triplet structure. Generate an explicit mapping to the library's internal indices; do not assume whether its index base equals ETS numbering.

| Channel | Switch object / DPT 1.001 | Block object / DPT 1.003 | Status object / DPT 1.002 |
| --- | --- | --- | --- |
| 1 | 1 | 2 | 3 |
| 2 | 4 | 5 | 6 |
| 3 | 7 | 8 | 9 |
| 4 | 10 | 11 | 12 |
| 5 | 13 | 14 | 15 |
| 6 | 16 | 17 | 18 |

Switch: communication/write enabled; read/transmit/update/read-on-init disabled. Block: same flags, 1 blocks, 0 releases. Status: communication/read/transmit enabled; write/update/read-on-init disabled. Status 1 means energized commanded output, 0 means de-energized. GroupValueRead on the status object returns that value. Defaults are design choices; qualify object flags with the stack and ETS.

Switch telegrams become commands to the common controller, not direct pin writes. Block preserves current output, rejects ordinary switch commands from every origin and does not buffer them for later replay. Reset, peripheral disable and an enabled disconnect safety policy may force OFF despite a block. After unblocking, wait for a new command. A web installer may explicitly clear a block and the KNX status model must reflect it.

After applied changes from KNX, web or button, update and optionally transmit the status object once. Never echo status back as a switch command. Suppress unchanged repetitive telegrams and rate-limit status bursts after reconnect. Disabled channels ignore switch actions, remain OFF and return OFF status; their configured objects remain stable rather than renumbering the product.

## Parameters and web commissioning

The application model includes per-channel enabled/startup/disconnect policies, timeout and pulse duration; global local-control permission and status-transmit policy. Parameter offsets and widths must be generated from one schema shared by XML, C++ accessors and TypeScript validation. Product settings not intended for ETS (web users, Wi-Fi secrets, protocol selection) remain outside the ETS parameter memory.

Proposed packed parameter layout, version 1, to be validated by the generator:

| Byte offset | Field |
| --- | --- |
| 0 | Parameter format version = 1 |
| 1 | Global flags: bit 0 local relay control enabled, bit 1 transmit status on change; remaining bits zero |
| 2–3 | Reserved, zero |
| 4 + 8c | Channel enabled, uint8 0/1, c=0..5 |
| 5 + 8c | Startup state, uint8 0/1 |
| 6 + 8c | Disconnect policy, uint8 0 hold / 1 OFF |
| 7 + 8c | Reserved, zero |
| 8 + 8c .. 9 + 8c | Disconnect timeout seconds, uint16 big-endian |
| 10 + 8c .. 11 + 8c | Pulse duration ms, uint16 big-endian |

Total 52 bytes. No C++ struct padding is part of the storage contract. GUI labels, ranges and defaults must match firmware. The web dashboard permits viewing/editing the individual address, these parameters and object/group associations, including multiple receive addresses and a designated sending address per object.

Validate individual addresses as area 0–15, line 0–15, device 1–255 for a commissioned end device. Validate three-level group addresses (main 0–31, middle 0–7, sub 0–255), reject reserved group zero, duplicates and table overflow. Proposed limits: 64 distinct group addresses, 96 associations, at most 8 receive associations per object and one transmit association. Confirm the pinned stack's table capacities before freezing the product XML. Show address in text and encoded form and use deterministic association ordering.

### Avoiding ETS versus web configuration divergence

Maintain one canonical commissioning image: device address, parameter bytes, group-address table, association table, object flags and application identity. Web forms read a decoded snapshot of this image, not a separately maintained address list. `configOwner` is `ets` by default or explicitly `web`.

- In ETS ownership, web displays everything and provides a deliberate installer-only **Take over for web editing** operation before editing. Explain that later ETS download can replace local edits; do not claim the device can update an ETS project file remotely.
- Web ownership uses a validated candidate image and revision, pauses group application traffic, writes through a tested stack adapter, persists a complete generation and restarts/reloads the application as needed. Restore the previous image if any table update fails.
- An ETS download acquires an exclusive commissioning lock and invalidates pending web edits. Publish progress, accept no concurrent table edits, validate the completed image and then update the shared snapshot and revision. After success ownership returns to ETS. Interrupted download does not activate partial parameters.
- Export a human-readable commissioning report and local backup with product/schema identifiers. Import only matching schemas and validate all tables before activation. This backup is not an ETS project synchronization format.

The stack adapter must access address/association objects and memory lifecycle using supported APIs or a reviewed, pinned extension. Both ETS writes and web writes must update the same storage representation. This compatibility spike is a release gate: merely changing a JSON field does not implement web KNX commissioning.

## Required knxprod source and build pipeline

Planned release tree:

```text
knx/product-model.json                       canonical identity/parameters/objects
knx/Edge_S3_Relay_6CH.xml                    generated ETS product source
src/generated/KnxProduct.h                  generated offsets/object mapping
interface/src/lib/generated/knx-product.ts  generated web schema
dist/Edge_S3_Relay_6CH_<application>.knxprod  generated and ETS-tested artifact
dist/knx-manifest.json                      input hashes/tool versions/test record
```

These paths are planned deliverables, not files created by this design review. Use a qualified pinned generator such as [OpenKNXproducer](https://github.com/OpenKNX/OpenKNXproducer), checking its documented input/signing requirements. The Sonoff project's XML is a structural reference; it cannot simply be duplicated with two extra names. Generate consistent catalog, hardware/product/application links, unique IDs, parameter types/references, dynamic channel UI, 18 object references, load procedures and address/association capacities.

Obtain appropriate manufacturer and product/application identifiers before producing a release identity; use clearly labeled development identifiers only in local experiments. Match firmware mask/application/version/parameter memory to the generated product. Increment the application version when offsets, object meanings or loading behavior change. Use the generator's supported packaging/signing flow rather than manually zipping XML, and record exact commands once the tool/version is selected.

The generator documentation requires an ETS installation and compatible converter/runtime. Its `check` and `create` operations provide structural checks and packaging, but successful generation still requires a separate ETS import/download test. Include that tool environment in the reproducible release record.

Acceptance requires ETS import in the chosen ETS version, discovery, individual-address programming from both web and BOOT activation, full application download, all six switch/block/status triplets, parameter changes, group-address associations, restart persistence, interrupted download recovery and supported unload behavior. Verify emitted traffic and coexistence with the LAN's KNX router/ETS interface. Do not carry the reference project's unload limitation forward silently: either implement it or document the tested restriction in the released product.

The requested `.knxprod` remains an explicit implementation deliverable. This review supplies the model and generation/validation contract; it does not claim certification or an ETS-valid binary without those steps.
