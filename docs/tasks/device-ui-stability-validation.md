# Device UI stability validation — 2026-10-07

Target: Waveshare ESP32-S3-Relay-6CH, MAC `CC:BA:97:34:D9:AC`, `http://device.example`, USB COM20.

The [agent prompt](device-ui-stability-agent-prompt.md) was saved before implementation.

## Findings and changes

- The status updater replaced entire nested arrays whenever a channel changed. Recursive field reconciliation now preserves channel arrays, channel objects and indicator objects, changing only supplied values that differ. Configuration reloads also retain object identity.
- Each command previously waited for a complete status fetch. Successful command replies now include device state; individual commands no longer request another status snapshot. Pending state remains scoped to the command. Live events continue during REST requests, and a generation guard prevents an older poll from overwriting newer shared state while retaining authenticated permission updates.
- Repeated failed WebSocket attempts emitted repeated loss notifications and reset backoff immediately on a handshake. A continuous outage now generates one loss notification; recovery requires a valid frame, and heartbeat acknowledgement resets backoff. The existing single-socket lifecycle, explicit ping/pong and cleanup remain enabled.
- WebSocket sends ran under actuator and subscription locks. Payloads are copied into bounded queued work and sent on the HTTP task without those locks. Hardware testing caught MessagePack truncation through Arduino String serialization; explicitly sized byte buffers now preserve embedded zero bytes.
- USB diagnostic writes could block while the host stopped consuming serial output. Actuator USB writes now use a zero transmit timeout, allowing diagnostic output to be dropped rather than delaying device tasks. This is a source-identified contention risk; not every observed network delay was independently attributed to it.
- The existing All enabled channels ON command was retained and placed beside OFF. Bulk commands preflight enabled channels, permissions and blocks before changing any relay. Existing all-channel target support was preserved.
- OTA restart no longer performs synchronous Wi-Fi teardown inside the HTTP callback. A verification-helper error was also corrected: authentication sessions contain a boot nonce, so post-reboot verification must log in again.

## Software checks

- Svelte: zero errors and zero warnings.
- Simulator/socket/reconciliation: 21 passed.
- Localization: 2 passed.
- Chromium: 6 targeted tests passed, covering control/DOM/edit stability, no command-triggered status GET, all-channel binding, quiet telemetry, reconnect/recovery, and delayed REST responses during live updates.
- Native gesture, Modbus and network checks passed; 3 product contract tests passed; Home Assistant native checks passed.
- Firmware build passed. RAM 55,692 / 327,680 bytes (17.0%); application flash 2,038,050 / 3,342,336 bytes (61.0%).
- `git diff --check` passed. Existing staged and unrelated changes were retained.

## Final firmware and OTA

- Version: **0.6.2**.
- Application image: `build/release/ESP32-Sveltekit_waveshare-relay-6ch_0-6-2.bin` (2,091,456 bytes).
- SHA-256: `e02e07b8f4b08ea34c1c88434a79364f1c95bd49abbeb5151e2ad2e62e744ba3`.
- MD5: `802216d99bf573a6828d66e93b866c6d`.
- Final OTA upload passed MD5 validation, returned HTTP 200, and rebooted automatically without USB recovery. Fresh authentication confirmed uptime 10 seconds, fault 0, all six relay states OFF, unchanged saved device configuration and a byte-for-byte matching embedded UI bundle.
- Earlier diagnostic attempts included USB reset recovery and an intermediate telemetry buffer correction. The final result above applies to the final image hash only.

## Hardware verification

Final browser monitoring is recorded in `.pio/ui-stability-062/browser-result.json`; OTA evidence is in `.pio/ui-stability-062/ota-result.json`, with the build log alongside them.

The physical RGB Identify command returned authoritative state without a redundant status GET. UI checks covered ON/OFF bulk controls, all-channel selection and stable indicator layout. Temporary local settings were discarded. Relay switching and bulk authorization were tested in the simulator; physical relay energization and electrical contact/load behavior were not tested.

Final 90-second run: **1 WebSocket connection, 0 disconnects, 91 state updates, 6 heartbeat replies, no offline warning and no page errors**. COM20 was briefly opened and closed without reset during the run and remained closed afterward. Heartbeat response times ranged from 53 to 321 ms. A bounded stability run does not establish indefinite connection reliability.
