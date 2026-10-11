# Xiaozhi MCP hardware validation — 2026-10-09

Target: Waveshare ESP32-S3-Relay-6CH, USB COM20, MAC `CC:BA:97:34:D9:AC`, device at `http://device.example`.

## Firmware and upload

- Built the current `dev` working tree with `platformio run -e waveshare-relay-6ch`.
- Version: `0.6.2`; application image: `build/release/ESP32-Sveltekit_waveshare-relay-6ch_0-6-2.bin`.
- Application size: **2,160,960 / 3,342,336 bytes**, leaving **1,181,376 bytes** in the OTA slot.
- SHA-256: `dc41372744109a52501e220cb33dcbdd80e66289dd164957c913c1833159746c`.
- Read the device partition table before upload and confirmed it matched the build byte-for-byte.
- Uploaded with `platformio run -e waveshare-relay-6ch -t upload --upload-port COM20`. Esptool verified the written data hash and reset the board. No filesystem upload or factory reset was performed.

## Hardware checks

- Administrator authentication works after upgrade. Saved actuator configuration matches the pre-flash JSON structurally, including channel labels and protocol settings.
- Device reports the `waveshare-relay-6ch` target and `xiaozhi_mcp: true`.
- KNX mode remains `knx_ip`, running with fault `0`. All six relay outputs remain OFF. No relay commands were sent.
- MCP initializes disabled, with no configured endpoint and channel mask `0`. Public settings contain no `endpoint` property.
- Default registry contains three tools: status, alias and identify. No tool was invoked on the hardware.
- All MCP GET endpoints and both POST endpoints reject unauthenticated requests with HTTP 401. Authenticated reconnect while disabled returns HTTP 409.
- All **11 embedded UI assets** served by the device match the decompressed assets in `WWWData.h` byte-for-byte.
- Chromium loads the embedded **Connections → Xiaozhi MCP** page, displays Disabled, the unconfigured endpoint and six channel choices. Screenshot saved in the evidence directory.
- Final single-browser run with the serial port closed: **13 successful samples over 65 seconds**, uptime advancing from 195 to 260 seconds, no reboot, no device fault, no browser page error, and MCP continuously disabled. Each three-request sample (system, actuator and MCP status) took **405–522 ms**. Sampled free heap was **138,288–141,956 bytes**; minimum free heap reported by firmware was **71,340 bytes**. These are disabled-MCP measurements, not TLS-load measurements. Final result: `validation.json` reports `passed: true`.

## Diagnostic observations and limits

Initial serial-port handling coincided with USB resets and temporary network unavailability. System status reported reset reason 11, which the installed ESP-IDF defines as `ESP_RST_USB`; the current UI labels it as unknown. Subsequent serial capture used explicitly deasserted DTR/RTS. Captures contained no Guru Meditation, task-watchdog or abort signature. Setup-password lines were redacted from saved serial evidence.

An observation run encountered a 10-second HTTP timeout while a second independent browser session was being opened. The overlapping run is retained as `validation-overlap.json`. This does not establish a root cause or multi-client stability. Browser-helper navigation/heading assumptions were also corrected during verification; those helper failures were not firmware failures.

MCP remained disabled throughout these checks. No endpoint was supplied, so cloud initialization, real TLS certificate rejection, enabled-MCP memory/latency, relay switching/pulse timing, MQTT/fieldbus coexistence under MCP traffic, OTA/reset scenarios and endurance are still unverified. KNX running with MCP disabled is only an upgrade smoke check.

## Evidence

Local evidence is stored under `.pio/mcp-hardware-20261009/`: upload log, partition table, before/after snapshots, redacted serial captures, browser screenshot, read-only validation helpers and JSON results. The build log is `.pio/waveshare-mcp-hardware-build.log`. Credentials are supplied to browser helpers through the environment and are not embedded in those files.
