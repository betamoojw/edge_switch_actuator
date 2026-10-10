# Xiaozhi MCP

The actuator can expose a small set of tools to Xiaozhi through an outbound, certificate-verified WSS connection. It runs alongside MQTT/Home Assistant and the selected Modbus or KNX interface. It does not change the protocol mode.

## Setup

1. Sign in as administrator and open **Connections → Xiaozhi MCP**, below MQTT and NTP.
2. Enter a device alias and your own Xiaozhi MCP endpoint (`wss://…`). The endpoint usually includes a credential. The firmware never supplies a shared/default credential.
3. Select the relay channels Xiaozhi may see and control. Initially none are exposed.
4. Enable Xiaozhi MCP and apply settings. The saved endpoint is hidden by default. Leaving its input blank retains the saved value; entering a new value replaces it. Administrators can check **Show endpoint** to retrieve the saved value explicitly. Revealing it alone does not change settings or reconnect MCP. Unchecking hides and clears the retrieved value; an edited replacement remains masked until saved or discarded.
5. Wait for **Ready**. Saving settings confirms persistence; Ready confirms the MCP initialization handshake. Configure NTP if the page shows **Waiting for time synchronization**.

Use Reconnect to reconnect with the saved configuration. Disabling the service preserves the endpoint. To remove the active endpoint, disable the service and select **Remove saved endpoint** in the same save. Factory reset erases both configuration generations. Credentials are stored in the device filesystem; this feature does not add flash encryption. A previous credential can remain in the inactive recovery generation until it is overwritten or factory reset.

Installer, operator and viewer accounts can see connection status. Only an administrator can read or change connection configuration, inspect the tool registry, or request reconnection. Endpoint values are excluded from status, GET responses, logs, and tool results. Do not enable WebSockets dependency debug logging in a credential-bearing build.

## Tools and relay behavior

Tool names end in the device's full 12-digit station-MAC identity, stable across WiFi/Ethernet changes. Aliases appear in descriptions and can contain Unicode; changing the alias reconnects and refreshes the registry. Channels use **1–6** externally.

| Tool prefix | Behavior |
| --- | --- |
| `actuator_status_` | Reports only exposed relay channels, revision, protocol mode and fault number. Protocol values are 0=off, 1=Modbus RTU, 2=Modbus TCP, 3=KNX. |
| `actuator_get_alias_` | Returns the configured alias and stable device identity. |
| `actuator_set_relay_` | Requires `channel` and boolean `on`. |
| `actuator_pulse_relay_` | Requires `channel` and `request_id`; uses the channel's configured pulse duration. |
| `actuator_all_off_` | Preflights and turns off only exposed, enabled channels. A blocked affected channel rejects the entire operation. |
| `actuator_identify_` | Uses the existing five-second identification indicator; fails if RGB is disabled. |

Relay mutation tools are omitted when no channels are exposed. All commands run on the actuator task and respect disabled/blocked outputs. They cannot unblock a relay, change a protocol, reset the device, install firmware or edit credentials. Command source is `xiaozhi_mcp`; existing browser state, HA state and KNX status publication paths reflect changes.

Pulse operation IDs are retained for 60 seconds across transient reconnects, with 16 reserved entries. A full retry cache rejects new pulses instead of evicting an unexpired operation and risking a repeated pulse. Reusing an operation ID with different arguments or a different settings revision is rejected. No mutation is automatically replayed after reconnect, and exactly-once execution across reboot is not promised. A response may be lost after a command has already applied; query status before issuing a new operation ID.

Existing fieldbus/network disconnect policies remain authoritative. Losing the Xiaozhi connection alone does not turn relays off.

## Runtime and troubleshooting

States include Disabled, Waiting for network/time, Connecting, Initializing, Ready, Retrying, Error and Paused. Provisioning AP alone is not an outbound uplink. The service follows the selected IPv4 WiFi/Ethernet interface. DNS names and IPv4 endpoint hosts are supported; IPv6 literals are rejected.

Production accepts only `wss://`, checks hostname/certificate trust using the embedded CA bundle, and requires plausible system time. There is no insecure TLS toggle. `authentication_failed` means an HTTP 401/403 handshake rejection; replace the endpoint with a valid one. `connection_failed` covers DNS/TCP/TLS, handshake/heartbeat and transport failures; check network, time, hostname and certificate trust. Raw remote error text is not exposed. Authentication rejection retries at roughly 60 seconds; other retries use jittered exponential backoff from 1 to 60 seconds.

TCP connect and TLS handshake timeouts are explicitly configured to five seconds each; DNS resolution uses the Arduino network stack. A small adapter establishes the verified secure client before handing WebSocket framing to the pinned library, avoiding its two-minute default TLS handshake timeout. Library automatic reconnect is suppressed; the service owns each new attempt. The MCP initialization deadline is 20 seconds. These are configured limits, not measured real-time guarantees.

`configuration_invalid` preserves a corrupt or unsupported stored record rather than overwriting it. Recover through an intentional factory reset. `storage_failed` leaves the prior active settings intact. Concurrent edits return a revision conflict; reload saved settings before retrying.

OTA sets an internal pause flag and waits up to three seconds for MCP transport shutdown before starting. If a connection attempt is still stopping, OTA reports an error so it can be retried. An unsuccessful OTA resumes MCP; reset/restart/sleep stop accepting commands. This lifecycle is independent of any browser being connected.

Limits: 2048 endpoint bytes, 64 UTF-8 alias bytes, 8192-byte assembled inbound MCP messages, nesting depth 8, 16384-byte responses, 49152-byte per-dispatch ArduinoJson allocation budget, four queued MCP operations and two-second execution deadlines. The pinned transport library separately bounds raw frame allocation to 15 KiB. Tool calls are limited to 10/second with burst four. Hardware heap/stack/latency measurements remain required before a production release.

## API

Argument-free MCP tools accept omitted `arguments` as well as `{}`. Request
metadata such as `_meta` is separate from a tool's input schema. See the
[status-tool parameter compatibility fix](tasks/xiaozhi-mcp-tool-parameters-fix.md)
for the regression and validation record.

For settings-save reboots reporting `Stack canary watchpoint triggered (httpd)`,
see the [2026-10-10 crash fix and hardware regression](tasks/xiaozhi-mcp-stack-overflow-fix.md).
The HTTP server now reserves at least 8192 stack bytes. Authenticated system
status exposes `http_stack_min_free_bytes` to measure the lifetime minimum
remaining HTTP stack after saves and reconnects.

| Route | Permission |
| --- | --- |
| `GET /rest/xiaozhiMcpStatus` | Authenticated |
| `GET`, `POST /rest/xiaozhiMcpSettings` | Admin |
| `GET /rest/xiaozhiMcpTools` | Admin |
| `POST /rest/xiaozhiMcpReconnect` | Admin; returns 202 when queued |
| `POST /rest/xiaozhiMcpEndpoint` | Admin; explicit saved-endpoint reveal, body `{ "revision": <current revision> }` |

The reveal response contains `endpoint` and `revision`, with `Cache-Control: no-store`.
Stale revisions return 409 and malformed requests return 422. Ordinary settings
and status responses remain redacted. The UI discards revealed saved values on
hide, reload, successful save, tab hiding and navigation, and never puts them
in local storage. A new unsaved replacement remains a draft, masked when hidden.

Settings POST accepts `revision`, `enabled`, `alias`, `channel_mask`, optional write-only `endpoint`, and optional boolean `clear_endpoint`. Omitted/empty endpoint preserves it. Clear and replacement cannot be combined; enabling requires a configured endpoint. GET returns `schema_version`, revision and editable public fields plus `endpoint_configured` and `endpoint_host`. Do not send the GET-only fields back in POST.

HTTP statuses: 400 malformed body, 401 no authentication, 403 insufficient permission, 409 revision/state conflict, 422 invalid setting, and 503 storage/service unavailability. Public error objects contain stable `error` and `field` values without echoing secrets. Feature-off firmware does not register these routes.

## Build and development

`FT_XIAOZHI_MCP` defaults to zero and is enabled for `waveshare-relay-6ch`. Runtime remains disabled. `FT_SECURITY=1` and `FT_NTP=1` are required; `SERVE_CONFIG_FILES` is forbidden with MCP. MQTT is independent. `links2004/WebSockets@2.7.2` is pinned. The MCP implementation is project-native; no WLED/vendor wrapper source or licensing/trial logic is copied.

Framework settings/status/lifecycle endpoints are consolidated in `XiaozhiMcpService` rather than three separate classes. Portable protocol/settings and transport modules remain separate; `XiaozhiMcpAdapter` owns the bounded actuator queue. DurableStore is shared from `lib/framework` with a forwarding header at its previous path.

Run native coverage with `python scripts/test_xiaozhi_mcp.py`. Build the product and regression variants with:

```text
pio run -e waveshare-relay-6ch -e waveshare-relay-6ch-mcp-off -e waveshare-relay-6ch-mcp-no-mqtt
```

The UI simulator never connects to Xiaozhi. Its authenticated control API accepts `{ "type": "mcp", "state": "ready" }` and other deterministic states. `mcp-only` and `mcp-off` profiles exercise navigation availability; they are UI fixtures, not valid production build configurations. Frontend contract tests cover settings, redaction, authorization, persistence failure and reset.

The constrained `esp32dev` and `esp32-wt32-eth01` profiles use link-time optimization to retain their existing features and dual-OTA partition layouts. Run `python scripts/check_firmware_size.py <environment>` after building: this checks the actual binary against the generated application slots, since the linker estimate can omit image sections. CI runs this check for every firmware target.

For an isolated test board, `interface/scripts/mock-xiaozhi-mcp.mjs` supplies a local WSS peer. Set `MCP_TEST_CERT` and `MCP_TEST_KEY`; optionally set `MCP_TEST_BIND` and `MCP_TEST_PORT`. The test firmware must trust that test certificate; production TLS verification is never disabled. The peer initializes MCP, lists tools and calls read-only status. It does not operate relays. Native fake-transport tests validate framing and that the verified-bundle API is configured; they do not prove real certificate rejection or cloud interoperability.

Release validation still needs a user-provided endpoint, an isolated board, invalid-certificate/hostname tests, protocol coexistence and OTA/reset checks, plus a 24-hour/100-reconnect soak. Record actual measurements and results; offline mocks and successful builds do not replace these checks.

## Validation record — 2026-10-09

Tested the uncommitted `dev` implementation based on `7fd6d69ce46ed5d1993b9feff164dae61721b21b` on Windows. The initial software-validation pass below did not upload firmware or contact a cloud endpoint or physical relay. A later user-authorized USB upload and hardware smoke checks are recorded in [Xiaozhi MCP hardware validation](tasks/xiaozhi-mcp-hardware-validation.md).

| Check | Result |
| --- | --- |
| Existing native actuator, networking, durable storage and product contracts | Passed |
| Existing Home Assistant adapter tests | Passed |
| Production MCP protocol/settings, relay policies and pulse retry cache | Passed, including all six channel mappings, blocked/disabled/masked rejection, strict JSON parsing, retry capacity and timer wraparound |
| Production transport with fake WebSocket API | Passed framing/fragment limits, authentication error classification, failure signaling and verified CA-bundle configuration |
| OTA pause helper and compile guards | Passed shutdown acknowledgement/timeout and rejected MCP builds without security/NTP or with public config serving |
| Simulator suite | 25 passed |
| Svelte/TypeScript check | 0 errors, 0 warnings |
| Translation checks | 2 passed; English plus six complete translated catalogues |
| Production frontend build and bundle isolation | Passed; JS 259.44 kB gzip, CSS 18.60 kB gzip; no simulator/control/test markers |
| Production Chromium regression suite | Initial run: 54 passed and one outdated REST-recovery test failed. That test now triggers the required WebSocket disconnect and passes on rerun. All three MCP cases also pass against the production bundle. |
| MCP browser matrix | All three MCP cases pass in Chromium, WebKit and mobile, including secret retention/removal, role/feature guards, standalone navigation and failed-save draft retention |
| Firefox | Installed runtime cannot launch on this host (`spawn UNKNOWN`); retained in the Linux CI matrix |
| Waveshare product firmware | Passed; actual image 2,160,960 / 3,342,336 bytes. PlatformIO reports 55,908 / 327,680 static RAM bytes; runtime TLS heap/stack remains unmeasured. |
| MCP-off and MCP-on/MQTT-off firmware | Both passed; linked symbols confirm MCP is excluded when disabled and retained when MQTT is disabled |
| Generic ESP32-S3 and Kincony firmware | Both passed with MCP disabled |
| ESP32 and WT32 Ethernet firmware | Both passed after enabling compile/link LTO. Actual OTA headroom: ESP32 56,512 bytes; WT32 8,192 bytes. Keep the binary-size CI guard enabled; WT32 has little expansion room. |
| C3 smoke build | Not completed locally: the new RISC-V toolchain download stalled; task-owned installer stopped after no download progress. The target is included in CI. |

The local WSS peer is supplied for subsequent board testing. Real certificate rejection, Xiaozhi initialization compatibility, actuator-task queue timing/cancellation under physical load, protocol coexistence, OTA/reset behavior on a board and endurance remain release validation items. They are not inferred from the simulator or fake transport.
