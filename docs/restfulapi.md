# REST API and events

This reference describes the actuator profile at `dev` commit `ced3e6d`.
The implementation is
[`ActuatorApi.cpp`](https://github.com/betamoojw/edge_switch_actuator/blob/ced3e6d8588f5bc3650d31cdd54e1f80f8bc06e9/src/device/ActuatorApi.cpp).
Feature-dependent framework routes disappear when compiled out.

## Authentication and permissions

`POST /rest/signIn` accepts a JSON object containing `username` and `password`.
Success returns `access_token`; failure returns 401. Send subsequent requests
with `Authorization: Bearer <access_token>` and JSON requests with
`Content-Type: application/json`.

The actuator's initial account is `admin` with its per-device setup password.
Tokens are limited to eight hours and the current boot. Administrative account
edits rotate the signing secret. `GET /rest/verifyAuthorization` returns 200 or
401. These routes do not add HTTPS to the device's HTTP server.

Authenticated device GET responses include `capabilities` with `configure`,
`command`, `admin` and `channels`. The channel bitmask uses bit 0 for channel 1
through bit 5 for channel 6. A mask of 63 permits all six. REST command channel
indices are **0–5**, while UI labels and MCP/MQTT channels are **1–6**.

## Actuator routes

| Method and route | Permission | Contract |
| --- | --- | --- |
| `GET /rest/device/status` | Authenticated | Relay state, network/protocol state, indicators, counters, faults and audit entries |
| `GET /rest/device/config` | Authenticated | Complete schema-version-1 configuration and current revision |
| `POST /rest/device/config` | Installer/admin | Complete validated configuration with current general revision |
| `POST /rest/device/commands` | Operator/installer/admin | Runtime command, with additional command-specific checks below |
| `POST /rest/protocol/transition` | Installer/admin | `mode` and current general `revision`; retains other settings |
| `GET /rest/knx/config` | Authenticated | KNX image/ownership/mapping snapshot with its own revision |
| `POST /rest/knx/config` | Installer/admin | KNX commissioning transaction; see [address rules](knx-address-entry.md) |
| `POST /rest/knx/programming` | Installer/admin | Boolean `active`; requires KNX mode and an IPv4 uplink |

The implementation also registers GET on command/transition/programming paths,
where it returns a device snapshot. Use `/rest/device/status` for ordinary status
reads. A POST object larger than 8,192 serialized bytes or a non-object receives
400. Mutations are serialized through the actuator queue; full queue returns 503.

### Relay examples

To turn **channel 1** ON, send this body to `/rest/device/commands`:

```json
{"command":"relay","channel":0,"value":true,"requestId":"panel-0001"}
```

To pulse channel 1 using its configured duration:

```json
{"command":"pulse","channel":0,"requestId":"panel-0002"}
```

These examples operate outputs when submitted to real hardware. Successful
command responses include `ok`, `error`, the general `revision` and a `state`
snapshot. Disabled/blocked relay commands return 409; a denied channel returns 403.

### Commands

| `command` | Fields and additional restrictions |
| --- | --- |
| `relay` | `channel` 0–5, boolean `value`; channel permission required |
| `pulse` | `channel` 0–5; channel permission required; uses configured pulse duration |
| `all_on`, `all_off` | Preflight every enabled channel's permission and block state; reject entire operation with 403 if any fails |
| `rgb` | `red`, `green`, `blue` 0–255; `brightness` 0–100 (default 10), `seconds` 1–30 (default 5); RGB enabled |
| `identify` | Existing five-second identify indication; RGB enable and higher-priority indicators still apply |
| `tone` | `hz` 500–4,000 (default 2,000), `ms` 10–2,000 (default 100), `duty` 1–50 (default 25); buzzer enabled |
| `acknowledge` | Silences current tone |
| `unblock` | Installer/admin; `channel` 0–5 |
| `modbus_window` | Installer/admin; `seconds` 0–300 (default 60), `peer` (default `rtu`); zero closes the window |
| `factory_reset` | Admin flag and `confirm` equal to `ERASE`; erases configuration and restarts |

RGB, tone and identify permissions are role-based rather than per-relay grants.
`unblock` is installer/admin restricted but does not check that user's channel
mask. See [operation](device-operation.md) for the permission model's scope.

### Revisions and retries

Read `/rest/device/config`, modify the complete configuration, then POST with its
current `revision`. The parser expects six relay records, three click bindings
and all required typed fields. A successful commit increments the revision.
Protocol transition is the narrow alternative:

```json
{"mode":"modbus_tcp","revision":1,"requestId":"mode-change-0001"}
```

Replace `1` with the revision actually read. Modes are `off`, `modbus_rtu`,
`modbus_tcp` and `knx_ip`. Configuration/transition responses report `ok`, `error`
and `revision`. Re-read configuration after success. KNX saves return a committed
`knx` snapshot and **KNX's own revision**, independent of general configuration.

Optional `requestId` is at most 64 characters. The firmware remembers 16 completed
operations for up to 60 seconds, keyed by username and ID. An identical path and
serialized payload returns the prior response; reusing the ID for a different
request returns 409. This bounded cache can evict entries and is lost on reboot.
It is not durable exactly-once execution. After a lost response, query status
before deciding whether a new operation is safe, especially for pulses.

| Status | Typical meaning |
| --- | --- |
| 200 | Read or completed operation |
| 400 | Malformed/oversized request body |
| 401 | Authentication required or invalid token |
| 403 | Role/channel permission denied, blocked bulk command, or unsupported command |
| 409 | Revision/state conflict, disabled/blocked single channel, or apply failure |
| 422 | Invalid field/schema/range |
| 503 | Request queue unavailable/full |

Early authentication/shape/queue failures may have empty bodies. Apply failure
can mean persistence or protocol startup failure as well as stale revision;
inspect the error string before retrying.

## Framework and integration routes

| Routes | Access / purpose |
| --- | --- |
| `GET /rest/features` | Public; compiled features and firmware identity |
| `GET /rest/wifiStatus`, `/rest/apStatus`, `/rest/systemStatus` | Authenticated status |
| `GET`, `POST /rest/wifiSettings`, `/rest/apSettings` | Admin network configuration |
| `GET /rest/scanNetworks`, `/rest/listNetworks` | Admin asynchronous Wi-Fi scan |
| `GET /rest/mqttStatus`, `/rest/ntpStatus` | Authenticated; feature dependent |
| `GET`, `POST /rest/mqttSettings`, `/rest/ntpSettings` | Admin; MQTT includes opt-in Home Assistant discovery |
| `GET`, `POST /rest/ethernetSettings`; `GET /rest/ethernetStatus` | Ethernet profile only; admin settings / authenticated status |
| `GET`, `POST /rest/securitySettings` | Admin; public serialization blanks password/signing-secret fields |
| `GET /rest/generateToken?username=<name>` | Admin token generation |
| `POST /rest/restart`, `/rest/factoryReset` | Admin lifecycle operations |
| `POST /rest/uploadFirmware` | Admin multipart firmware upload |
| `POST /rest/downloadUpdate` | Admin; JSON `download_url` for an OTA application image |
| `GET /rest/coreDump` | Authenticated crash diagnostic download |

`/rest/sleep` belongs to the optional sleep feature and is absent on the actuator
profile. For integration-specific schemas, redaction rules and all five MCP
routes, see [Xiaozhi MCP](xiaozhi-mcp.md). Broker topics and authorization are
covered by [Home Assistant](home-assistant.md).

## Event socket

`/ws/events` authenticates at connection admission. The default wire codec is
binary MessagePack; `EVENT_USE_JSON=1` selects text JSON. Logical envelopes use
`event` and `data`. Subscribe with:

```json
{"event":"subscribe","data":"device.state"}
```

`device.state` is emitted approximately once a second and is read-only in the
actuator application. Use REST commands for mutations. Unsubscribe uses the same
shape with `event: "unsubscribe"`. Application heartbeat `{"event":"ping"}`
receives `{"event":"pong"}`. Frames above 8,192 bytes are rejected; outbound
pending work is bounded.

On reconnect, get a fresh REST snapshot and resubscribe. The frontend's
`lib/device/reconcile.ts` preserves unrelated state when applying partial updates.
Existing sockets are not documented as being closed immediately when an account
is edited; see [source review](actuator-source-review.md) for that boundary.
