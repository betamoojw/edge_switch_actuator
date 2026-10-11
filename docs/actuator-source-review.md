# Current dev source review

Reviewed on **11 October 2026**, against `dev` commit
[`bf8cb12ab9ada4337751968332c7d5e17e056ccc`](https://github.com/betamoojw/edge_switch_actuator/tree/bf8cb12ab9ada4337751968332c7d5e17e056ccc),
firmware version **0.6.4**. The remote `dev` head matched this baseline at the
initial source review; subsequent documentation commits do not change that code.
This is a source review with selected software regressions, not a comprehensive
security audit or a new hardware/ETS qualification campaign.

This page supersedes the September 2026 template-only review. The actuator,
Modbus, KNX, credentials, MQTT and MCP implementations now exist. The
[original production design](actuator-production-design.md) remains useful as
historical intent; [architecture](architecture.md) describes actual code.

## Changes since the 10 October review

Firmware 0.6.4 fixes the GitHub release API repository identifier and replaces the
Discord sidebar link with the project documentation link. The checked-in default
board binaries are now named 0.6.4, and `buildRelease/Edge_S3_Relay_6CH.knxprod`
is included. No backend API or actuator behavior changed between the previous
`ced3e6d` baseline and this source baseline. See [release notes](release-notes.md).

The [10 October review](source-review-2026-10-10.md) preserves the earlier findings,
including the C3 size failure from that commit. It is historical evidence, not a
new build result for 0.6.4. No device update or active hardware test was performed.

## Findings still present at the reviewed baseline

| Priority | Evidence | Impact and next change |
| --- | --- | --- |
| P1 | `lib/framework/FSPersistence.h`, `writeToFS()` opens the active file with `w`; read failure applies and writes defaults | Generic Wi-Fi/security/MQTT settings can be lost after an interrupted write. Unlike actuator/KNX/MCP storage, these files do not use verified generations. Migrate them to durable commits and test interrupted writes and recovery. |
| P1 | `UpdateIndicator.svelte` and `GithubFirmwareManager.svelte` select assets using `.bin` and board-name substring checks; `scripts/release_artifacts.py` emits both OTA and merged binaries | If both image kinds or related MCP profiles appear in a release, the wrong image can be selected. Match the exact target and `_ota.bin` suffix, with explicit artifact metadata/validation. |
| P2 | `scripts/build_interface.py`, `find_latest_timestamp_for_app()` scans only `interface/src/` | Static assets, package/lock files and Vite configuration can change without rebuilding the embedded UI. Include all build inputs in freshness tracking; delete the generated `WWWData.h` to force rebuilding in the meantime. |
| P2 | `factory_settings.ini` pairs `Europe/Berlin` with `GMT0BST,M3.5.0/1,M10.5.0` | Factory time-zone label and actual offset disagree. Select a matching NTP zone at provisioning and correct the default pair. |
| P2 | `lib/framework/EventSocket.cpp`, unsubscribe uses `client_subscriptions[doc["data"]]` without validating the event name | An authenticated client can create empty map entries for arbitrary unsubscribe names. Use a lookup/registered-event check and bound subscription keys. This is a static resource-exhaustion concern, not a reproduced remote failure. |
| P2 | `EventSocket::begin()` authenticates socket admission; `onFrame()` and outbound delivery do not re-check token lifetime/account state | Existing sockets can outlive REST session expiry or account edits. Define and test explicit socket revocation. Actuator `device.state` is read-only, but remains observable on an admitted connection. |

These are review findings, not firmware fixes made by this documentation refresh.
The remaining release-picker issue is visible from filename matching; no firmware
update was attempted. Source links for inspection:
[frontend layout](https://github.com/betamoojw/edge_switch_actuator/blob/bf8cb12ab9ada4337751968332c7d5e17e056ccc/interface/src/routes/%2Blayout.ts),
[update indicator](https://github.com/betamoojw/edge_switch_actuator/blob/bf8cb12ab9ada4337751968332c7d5e17e056ccc/interface/src/lib/components/UpdateIndicator.svelte),
[release manager](https://github.com/betamoojw/edge_switch_actuator/blob/bf8cb12ab9ada4337751968332c7d5e17e056ccc/interface/src/routes/system/update/GithubFirmwareManager.svelte),
[embedding script](https://github.com/betamoojw/edge_switch_actuator/blob/bf8cb12ab9ada4337751968332c7d5e17e056ccc/scripts/build_interface.py),
[event socket](https://github.com/betamoojw/edge_switch_actuator/blob/bf8cb12ab9ada4337751968332c7d5e17e056ccc/lib/framework/EventSocket.cpp).

The later [live inspection](live-verification.md) also found that the KNX helper's
fixed triple-click wording can disagree with configured button bindings, and
that the inspected 0.6.3 device had no MCP menu. The latter does not identify a
source defect without establishing the running image and embedded UI identity.

## Implemented improvements since the template review

- `src/main.cpp` sets relay GPIOs safe before service startup; product logic owns
  a dedicated actuator task, with queued HTTP mutations and bounded integration
  queues. Generic light examples are excluded from the actuator build.
- `NetworkSupport` derives readiness from the selected IPv4 interface and excludes
  AP-only provisioning. IP protocol listeners restart on uplink changes.
- Actuator REST handlers enforce roles and relay channel masks. The product state
  event has no registered write callback. Generic `EventEndpoint<T>` remains an
  extension abstraction; it should not be treated as a role-checking layer.
- Unique NVS setup identity replaces shared actuator defaults. Passwords use salted
  PBKDF2 hashes; security API serialization blanks password/signing-secret values.
  Tokens are boot-bound and limited to eight hours, with secret rotation on edits.
- DurableStore provides verified generations, chunked KNX image support and recovery.
  Filesystem startup preserves corrupt existing data; reset uses a durable marker.
- KNX web saves commit address, tables and parameters with rollback and their own
  revision. Ownership survives reboot; the package identity and parameters have
  contract checks.
- Event subscription mutation is mutex-protected, subscriptions are deduplicated,
  and sends are queued on the HTTP task with bounded pending work.
- MCP uses verified WSS, redacted settings, bounded parsing/queues, channel exposure,
  pulse retry protection, and OTA shutdown coordination. MQTT is independent.
- Firmware packaging uses current compiler outputs and checks partition fit in CI.

## Operational and qualification limits

Management remains HTTP. Modbus has no authentication or encryption; MQTT relies
on broker authorization. Channel masks are not a universal framework permission
model. At-rest flash encryption and signed-image authenticity are not established
by this code review. The commented OTA certificate bypass is **not enabled** in
the reviewed profile, unlike the old review's claim.

RTU uses UART idle events and CRC checks but does not implement exact rejection
of every intra-frame gap above 1.5 characters. KNX product generation does not
prove ETS import, full/partial download, device-specific registration or
certification. Electrical startup behavior, relay endurance, RS485 turnaround,
TLS rejection on hardware, resource limits under load and long-duration recovery
need qualification appropriate to the deployment.

The NTP label mismatch and generic branding are examples of remaining template
configuration drift. Some firmware dependency versions still use ranges. The
frontend freshness check and tracked release files mean a binary in the checkout
must not be assumed to match later source changes without rebuilding and hashing.

## Documentation defects corrected by this refresh

The landing page, quick start, build guide, frontend reference and API reference
now describe the actuator rather than the upstream light demo. This site links
to the correct repository and documents current factory credentials, roles,
protocol coexistence and versioned firmware output. Navigation includes UI
preferences and validation procedures that were previously hard to discover.

Documentation dependencies are pinned, strict link/anchor validation runs during
build, and the publication workflow targets `dev` through GitHub Pages artifacts.
Historical task records retain their original dates and scope; they are not
represented as checks rerun during this review. See [validation](validation.md).
