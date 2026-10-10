# Device operation

The browser opens at `/device`. Its sections cover Outputs, Indicators, Button,
Protocol Interface, KNX and Maintenance. Available controls depend on the current
user's capabilities and the compiled firmware features.

## Outputs

Channel labels in the UI run from **1 to 6**. Each channel has an enable flag,
startup state, name, pulse duration and disconnect-off timeout. A disabled channel
is forced OFF; a blocked channel rejects ordinary relay commands. An installer
or administrator can unblock channels. A pulse uses the saved per-channel
interval. Its expiry forces the output OFF; an ordinary accepted relay command
cancels its timer.

Bulk web ON/OFF commands preflight all enabled channels. A blocked enabled channel
or missing channel permission rejects the whole command. Disabled channels are
left OFF. Changes are reflected in device status and event snapshots; status is
commanded output state, not measured contact state.

Default startup is OFF. Choosing startup ON deliberately changes behavior after
restart. In KNX mode, once commissioned, edit relay parameters in the KNX section.
Reload after a revision conflict instead of overwriting newer settings.

## Protocol selection

| Mode | Prerequisites and behavior |
| --- | --- |
| Off | Fieldbus stopped; web control and enabled MQTT/MCP integrations remain available |
| Modbus RTU | RS485 enabled; default unit 1, 19,200 baud, 8E1 |
| Modbus TCP | Selected uplink has IPv4; default port 502; optional exact IPv4 peer restriction |
| KNX/IP | Selected uplink has IPv4; commission individual address, groups and parameters |

Only one fieldbus runs at a time. `waiting_network` means a selected IP protocol
is waiting for an uplink, not that saving failed. AP-only provisioning is not an
uplink. See [Modbus](actuator-modbus-map.md) and [KNX](knx-address-entry.md).

Disconnect-off defaults to disabled. For TCP/KNX, it responds to loss of the
selected network uplink after the configured timeout, not loss of a particular
master or telegram stream. For RTU it requires the optional bus watchdog and is
based on bus inactivity. MQTT/MCP disconnection alone does not turn outputs off.

## Button and indicators

Single, double and triple clicks have configurable bindings. Defaults are no
action, identify, and KNX programming toggle respectively. Relay-action target 0
means all enabled channels; specific targets are 1–6. Local button actions skip
blocked/disabled channels. Turning off application button actions does not
disable the reserved factory-reset hold.

The recognizer debounces for 30 ms, accepts short clicks up to 700 ms, and groups
them after a 350 ms release interval. A hold arms reset at five seconds and
executes factory reset at ten seconds. A button held during startup must first
be released before gestures are accepted.

RGB indication priority is reset-armed, fault, KNX programming, manual identify
or test, connected uplink, AP provisioning, then waiting/connecting. RGB must be
enabled to display these indications. Manual RGB tests are limited to 1–30 s.
Buzzer tests require enablement, 500–4,000 Hz, 10–2,000 ms and 1–50% duty.
Acknowledging a tone silences it; it does not clear the underlying fault.

## Access roles

| Role | Actuator permissions |
| --- | --- |
| Viewer | Authenticated status/configuration reads |
| Operator | Runtime commands; relay writes limited by channel mask |
| Installer | Runtime commands, configuration, protocol/KNX commissioning, unblock and Modbus window |
| Administrator (`admin` flag) | All channels, plus user/network/integration administration and reset/update |

Channel masks constrain actuator relay commands; they are not a general grant
system covering every framework resource. Framework settings usually require an
administrator. MQTT authorization belongs to broker ACLs; MCP uses its own
exposed-channel mask. Neither inherits a browser user's channel mask.

## Maintenance and fault interpretation

Use **System → Firmware Update** for manual OTA with the matching `_ota.bin`.
See [build and firmware updates](buildprocess.md) for known GitHub release-picker
limitations. The configured profile disables deep sleep.

| Fault | Source meaning |
| --- | --- |
| 0 | No active fault recorded |
| 1 | Buzzer PWM allocation failed |
| 2 | Invalid saved actuator configuration; defaults loaded |
| 3 | Protocol initialization failed |
| 4 | Factory-reset marker or cleanup failure |
| 5 | KNX persistence failure; successful retry clears this fault |
| 6 | Invalid KNX application parameters |

Inspect `error`, protocol state and the in-memory audit log together. Factory
reset erases resettable configuration and KNX commissioning while preserving the
manufacturing setup password. Disable Home Assistant discovery while the broker
is reachable before removing/resetting a unit if retained discovery cleanup is
needed. See [Home Assistant lifecycle](home-assistant.md).
