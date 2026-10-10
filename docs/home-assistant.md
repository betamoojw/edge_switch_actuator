# Home Assistant integration

The `waveshare-relay-6ch` firmware supports Home Assistant through the existing MQTT connection. It needs no custom Home Assistant component. Discovery is opt-in and defaults to off, including when loading settings saved by older firmware. Template profiles retain their existing demo-light integration.

## Setup

1. Configure Home Assistant's [MQTT integration](https://www.home-assistant.io/integrations/mqtt/) with your broker and enable discovery using the default `homeassistant` prefix and `homeassistant/status` birth topic.
2. Sign in to the actuator as an administrator. Under **Connections → MQTT → Change MQTT Settings**, set the same broker URI and credentials, enable MQTT and **Home Assistant discovery**, then apply.
3. Home Assistant discovers one **Edge Switch** device. Relay names follow the actuator configuration; entity IDs remain stable when names or MQTT client IDs change.

Use a broker account/ACL that permits only trusted controllers to publish device commands. MQTT authentication is supplied by the broker; HTTP users and channel permissions do not apply to MQTT.

## Entities and topics

`<id>` is `edge_` followed by the factory station MAC address (12 lowercase hexadecimal digits). It remains stable across Wi-Fi/Ethernet uplinks. The base topic is `edge_switch/<id>`.

| Entities | Behavior |
| --- | --- |
| Six relay switches | Commands use the existing relay method, including enabled/blocked checks and KNX feedback. Disabled or blocked channels are unavailable. |
| Six channel-blocked binary sensors | Report blocked **or disabled** channels without granting an unblock capability. |
| Uptime, protocol, protocol status | Read-only diagnostic sensors. |
| Fault and button-pressed binary sensors | Current fault and debounced input state. Short presses may occur between the one-second snapshots; use gesture events for automations. |
| Identify button | Existing five-second identify action; RGB enable and indicator priority still apply. |
| Button gesture event | `single`, `double`, `triple`; emitted only when the configurable button function is enabled. Existing local bindings still run. Reset holds are not exposed as automation events. |

| Topic | Payload / retention |
| --- | --- |
| `homeassistant/<component>/<id>/<entity>/config` | Discovery JSON, QoS 1, retained |
| `<base>/state` | Device status JSON (without the private audit log), every second and after a command, QoS 1, retained |
| `<base>/relay/1/set` … `/relay/6/set` | Exact `ON` or `OFF`; publish without retention |
| `<base>/identify/set` | Exact `PRESS`; publish without retention |
| `<base>/event` | `{"event_type":"single"}` (or `double`/`triple`), QoS 1, **not retained** |
| Existing framework status topic | Retained `online` and last-will `offline`, shared by every discovered entity |

The discovery prefix and HA birth topic are fixed to the defaults in this version. Discovery and current state are resent on broker reconnect, Home Assistant's `online` birth message, and relay rename. Commands delivered with MQTT's retained flag are discarded. Malformed topics/payloads are ignored; no toggle, factory-reset, protocol-selection, or configuration-write command is exposed. The bounded 16-command queue drops excess commands. Snapshots report applied commanded output state, not measured contact or load feedback.

## Compatibility and lifecycle

The MQTT adapter runs alongside the selected Modbus RTU, Modbus TCP, KNX/IP, or off mode. It does not change protocol selection, startup outputs, pulse expiry, button bindings, disconnect-off policy, Modbus permissions or KNX commissioning. MQTT commands do not refresh the Modbus RTU bus watchdog. A later protocol/button command can change an output as before. Broker loss itself does not add a new relay-off policy.

Disabling **Home Assistant discovery** while MQTT remains connected clears the retained discovery records and snapshot. A marker under `/config/home-assistant-discovery` allows cleanup after reboot if discovery was disabled while disconnected. Keep the old broker reachable until cleanup completes. Changing brokers leaves retained records on the old broker; remove those there, or disable discovery before switching. Before factory reset or permanently removing a device, disable discovery while connected. Factory reset is intentionally unchanged and cannot clean an unreachable broker.

With `FT_MQTT=0`, the actuator adapter compiles out. The optional settings field is exposed only on the actuator profile. Existing REST endpoints and actuator configuration schema are unchanged; `/rest/mqttSettings` adds `home_assistant_discovery: false` for this profile.

## Review and validation

The [10 October read-only inspection](live-verification.md) found MQTT disabled;
it did not exercise discovery or Home Assistant. The records below are historical
software evidence, not current live integration acceptance.

Reviewed local `dev` baseline `16e5935`. The existing application task is the sole serialization point for relay and protocol work; networking callbacks must not write GPIO or acquire its lock. The new MQTT callback only queues validated commands or requests rediscovery. State is sampled inside the existing actuator task, covering REST, button, Modbus, KNX, pulse and watchdog changes without adding transport logic to each source.

Run `python scripts/test_home_assistant.py` after installing the PlatformIO project dependencies. It exercises the production MQTT adapter against fake transport/RTOS/hardware with the real ArduinoJson library: discovery structure, state filtering, command handoff, interlocks, retained-command rejection, non-retained gestures, rename, removal, publish failure retry, and reconnect. `python scripts/test_native.py` runs the portable command-parser, gesture/Modbus/network and product-contract tests. Frontend checks use `npm run check`, `npm run test:sim`, and `npm run test:i18n` from `interface`. The MQTT browser cases run with `npx playwright test --project=chromium --grep MQTT`.

Hardware acceptance still requires a broker and a Home Assistant instance: enable discovery, exercise all six relays from HA and each existing control source, block/disable channels, rename channels, restart HA and the broker, disconnect the device, and disable discovery. Confirm actual relay outputs and availability. No firmware flashing or live Home Assistant commissioning is performed by these host tests.

Validated locally: actuator and generic S3 firmware builds; native and adapter tests; Svelte check (zero errors/warnings); 13 simulator tests; two translation tests; and both Chromium MQTT settings tests. The generated embedded web interface was rebuilt with the new settings control.

Protocol references: [MQTT discovery](https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery), [MQTT switch](https://www.home-assistant.io/integrations/switch.mqtt/), and [MQTT event](https://www.home-assistant.io/integrations/event.mqtt/).
