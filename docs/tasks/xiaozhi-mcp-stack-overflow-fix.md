# Xiaozhi MCP settings-save reboot: diagnosis and validation

Date: 2026-10-10. Branch: `dev`, based on `cb1beb2`.
Hardware: Waveshare ESP32-S3-Relay-6CH, MAC `CC:BA:97:34:D9:AC`, `device.example`.

## Root cause

Both panics in the supplied `xiaozhi_mcp_debug.txt` report
`Stack canary watchpoint triggered (httpd)`. The log identifies ELF SHA-256
`ce22900bb8c94d2cee487ae1ecc44d530099e385a0df60132afc5597643673a9`.
The matching archived ELF in `build/elf` was decoded with the installed
`xtensa-esp-elf-addr2line -pfiaC` tool. Both backtraces have the same chain:

```text
PsychicJsonHandler::handleRequest
  XiaozhiMcpService::save
    DurableStore::write
      DurableStore::readSlot
        ArduinoJson deserializeJson(File)
          File::readBytes / fread / __smakebuf_r / fstat
            LittleFS directory lookup / block CRC
              esp_partition_read / esp_flash_read / flash lock
                HTTP task stack canary
```

The project inherited `HTTPD_DEFAULT_CONFIG().stack_size = 4096` from ESP-IDF.
An authenticated MCP settings POST synchronously validates, writes and verifies
the two-generation durable record. The nested HTTP, JSON, filesystem and flash
calls exceed that stack allocation. The fault occurs while inspecting a stored
generation, before the save finishes; it is not evidence of invalid endpoint
syntax, TLS certificate failure or a relay-driver fault. The separate MCP
worker already has an 8192-byte stack and was not the task named in the panic.

Before the fix, the board could boot from its saved settings and reach cloud
state `ready`; system status still reported the previous panic reset. This is
consistent with a settings-save path failure rather than a boot/connect failure.

## Fix

- `ESP32SvelteKit::begin()` sets a minimum HTTP task stack of **8192 bytes before
  starting the server**, retaining any larger caller-provided value. This costs
  an additional 4096 bytes of runtime RAM on targets using the default.
- Authenticated `GET /rest/systemStatus` now includes
  `http_stack_min_free_bytes`, the HTTP task's lifetime minimum free stack since
  server startup. ESP-IDF's `uxTaskGetStackHighWaterMark` returns bytes. It is a
  diagnostic field, not a live free-stack reading or the MCP worker watermark.
- Durable acknowledgement, revision checks, credential redaction and verified
  TLS behavior remain intact. No asynchronous persistence or storage-format
  migration was introduced.

## Build and deployment

Native MCP protocol/settings/relay-policy, transport, lifecycle and compile-guard
tests passed with `python scripts/test_xiaozhi_mcp.py`. Native actuator, network,
durable-storage and six product-contract tests passed with
`python scripts/test_native.py`.

`platformio run -e waveshare-relay-6ch` succeeded. The binary-size guard passed:
**2,160,720 / 3,342,336 bytes**, with **1,181,616 bytes** of OTA slot headroom.

- Application: `build/release/ESP32-Sveltekit_waveshare-relay-6ch_0-6-2.bin`
- Version remains `0.6.2`; identify this build by its hash.
- Application SHA-256: `b6b84fc2dbc1cb307ac1264ed4c1e72a335547c62bf2e85e0a8b5f86304d925a`
- Application MD5: `7e57e86b191a78b43a9934d339f12f02`
- ELF SHA-256: `c8db59a98d9417e69ac9b4902904e3063a5c0746138c6670635050b1897a2fd3`

Authenticated OTA returned HTTP 200 and the board restarted successfully.
Target/MAC checks passed. Actuator and KNX configuration matched their pre-OTA
snapshots. All 11 embedded UI assets matched the build byte-for-byte. No
filesystem image, factory reset or physical relay command was used.

## Hardware regression results

The existing secret endpoint was retained throughout. MCP relay permissions
were temporarily set to mask `0` during reconnect tests, then restored to `63`.
The original enabled state and alias were restored; revision increased normally.

| Check | Result |
| --- | --- |
| 12 consecutive settings changes using 64-byte aliases, alternating durable slots | Passed |
| No-op save | Passed; revision unchanged |
| Stale revision | HTTP 409; stored settings unchanged |
| 65-byte alias and insecure `ws://` endpoint | HTTP 422; stored settings unchanged |
| Unauthenticated settings POST | HTTP 401 |
| Three enable → ready → reconnect → ready → disable cycles | Passed against the configured real Xiaozhi cloud endpoint |
| Masked tool registry | Three tools; relay tools absent |
| Original configuration restoration and enabled observation | Passed; MCP ready |
| Uptime monitoring | 39 samples, uptime 34–135 seconds, final system snapshot 136 seconds; no unexpected reboot |
| HTTP stack lifetime minimum after saves | **3920 bytes** |
| Lowest sampled free heap | **73,592 bytes** |
| Firmware lifetime minimum heap at final snapshot | **25,312 bytes**, including transient activity |
| KNX and outputs | `knx_ip`, running, fault 0; all six relays stayed OFF; actuator and KNX configuration unchanged |

8192 minus the measured 3920-byte watermark is **4272 bytes of stack use**,
which exceeds the previous 4096-byte allocation. This measurement corroborates
the exact-ELF crash diagnosis and provides headroom evidence for the fix.

The final observation included twelve samples spaced five seconds apart after
restoring the original enabled configuration. This is a targeted crash
regression, not an endurance or full product-certification result. Physical
relay/pulse behavior, invalid-certificate rejection and a long reconnect soak
remain separate validation tasks.

The in-app browser initially showed a blank deep-link page with a JavaScript
route-not-found error; subsequent browser automation commands timed out, including a
fresh-tab attempt. Visual UI validation was therefore not completed in this
run. Device REST responses and the firmware-served asset contents were verified
independently. The REST settings POST tested above is the same operation used
by the UI Save action.

## Repeating the regression

1. Authenticate as an administrator and record MCP public settings, device
   configuration, KNX configuration, relay states and system uptime. Never
   export or log the secret endpoint.
2. Retain the endpoint by omitting `endpoint` from settings POSTs. Send the
   current `revision`, `enabled`, `alias` and `channel_mask` only.
3. Disable MCP and temporarily set mask `0`. Repeatedly change the alias up to
   its 64-byte limit, reading settings back after every save.
4. Enable MCP, wait for `ready`, POST reconnect and wait for `ready` again.
   Disable it and repeat. Send no actuator tool calls.
5. Between operations poll system, actuator and MCP status. Require monotonic
   uptime, unchanged relay states, KNX running with fault 0, and at least 2048
   bytes of HTTP minimum stack margin. Stop on a failure and record it.
6. Restore the original enabled state, alias and channel mask even if a check
   fails. Confirm the endpoint remains configured and observe the final state.

Local evidence (ignored by Git): `.pio/mcp-stack-fix-build.log`,
`.pio/ota-mcp-stack-fix/`, `.pio/mcp-stack-fix-validation/result.json`, and
`.pio/verify_mcp_stack_fix.py`. The helper reads the administrator password from
`DEVICE_PASSWORD`; saved reports contain neither it nor the endpoint token.

## Visual UI follow-up — 2026-10-10

Completed in the Codex in-app browser on the same hardware. The configured
hostname `esp32-ccba9734d9ac.local` was resolved to `device.example` before use.
The browser's IP-address origin retained an older JavaScript bundle with no MCP
route; the hostname loaded the current firmware-served interface. The device's
served bundle matched the local build (SHA-256
`820df0e06586312ce9fc1b20374c9a0bda60e9ebd9d008d0d179c37c1f461e49`).
The old IP-origin cache was not cleared or fixed in this follow-up.

All changes below were made with visible UI controls, not settings REST calls:

| Action | Observed UI result |
| --- | --- |
| Uncheck Enable Xiaozhi MCP, click Apply Settings | Settings saved; Disabled |
| Change alias to `MCP UI Verification`, click Apply Settings | Settings saved; status and field show new alias |
| Click Reload saved settings | Saved alias retained |
| Check Enable Xiaozhi MCP, click Apply Settings | Settings saved; Ready; six tools |
| Click Reconnect | Reconnection requested; Ready afterward |
| Restore `Switching Actuator`, click Apply Settings | Settings saved; Ready |
| Navigate away and reopen the MCP page | Original alias and enabled state retained; endpoint still saved; all six channel permissions unchanged |

System Status showed uptime **14 minutes 11 seconds** and reset reason
**Software reset via esp_restart**, unchanged from the OTA boot. The subsequent
actuator page showed **14 minutes 30 seconds**, `knx_ip` running and all six
outputs OFF. Before this browser verification sequence, the read-only baseline
recorded 410 seconds uptime; elapsed wall time and final uptime are consistent
with the same boot. No reboot occurred during the UI operations and no relay
commands were issued.

Two supplemental Python sign-in attempts timed out while the browser was open.
An initial partially loaded settings page recovered after reload. Therefore
this result confirms the requested single-browser actions, not concurrent-client
availability. Final uptime/reset and output checks were read visually from the
device UI rather than inferred from those unsuccessful API requests.

Screenshots and UI text evidence are saved under `.pio/mcp-ui-verification/`:
`ui-disabled.jpg`, `ui-alias-saved.jpg`, `ui-enabled-ready.jpg`,
`ui-reconnected.jpg`, `ui-restored-ready.jpg`, `ui-system-status.jpg`,
`ui-system-status.txt`, and `ui-device-status.txt`. The endpoint token was never
displayed or changed. No additional firmware changes or OTA update were needed.
