# Choose an integration

The browser works without a fieldbus. Select **one** fieldbus under the actuator's
Protocol tab: Off, Modbus RTU, Modbus TCP or KNX/IP. MQTT/Home Assistant and Xiaozhi
MCP can run alongside it. Their independent command sources share relay state;
the later accepted command can supersede an earlier one.

| Control path | Prepare | First verification | Limit |
| --- | --- | --- | --- |
| Browser / REST | Device address and account | Read dashboard or authenticated status | HTTP; user role and channel mask apply |
| Modbus RTU | RS485 master, matching unit/baud/parity, correct termination | Read FC04 offset 0, count 5 | Physical bus access is trusted; no per-master authentication |
| Modbus TCP | IPv4 uplink and allowed master, default port 502 | Same register read over TCP | Up to four clients; exact peer restriction is not encryption |
| KNX/IP routing | IPv4 multicast network and planned addresses | Inspect KNX snapshot and intended group associations | No KNX TP interface or tunneling-server promise; ETS acceptance outstanding |
| Home Assistant | MQTT broker, HA MQTT integration, discovery enabled | Check discovery and availability before commands | Broker ACLs govern access; browser role masks do not apply |
| Xiaozhi MCP | Credential-bearing WSS endpoint, working NTP, selected exposed channels | Wait for Ready, then use status tool | No exposed channels by default; no automatic replay of mutations |

## Modbus example: a read before a write

With the intended transport already selected and commissioned, configure your
master for unit **1**, then read **input registers** at zero-based offset
`0x0000`, quantity `5`. Expected fields are map version, capabilities, selected
protocol, network state and fault. Map version should be `1`; protocol should
be `1` for RTU or `2` for TCP. A timeout requires transport diagnosis, not a
write attempt. UI addresses such as `30001` vary by master; use its zero-based
offset mode when available.

FC01 offset `0`, quantity `6` reads commanded relay states. FC05 writes can
operate hardware: use `0xFF00` for ON and `0x0000` for OFF only in an authorized
test. The vendor demo's `0x5500` toggle is not supported. See the complete
[register map](actuator-modbus-map.md), [verifier](modbus-verifier-quickstart.md),
and transport-specific [RTU](modbus-rtu-relay-fat-sat.md) /
[TCP](modbus-tcp-relay-fat-sat.md) acceptance plans.

## KNX example: separate command and feedback

Plan a unique individual address, such as `1.1.20` if available in your topology.
For channel 1, object 1 is Switch, object 2 Block and object 3 Status. An example
association is `1/0/1` for Switch and `1/1/1` for Status, leaving Block unassigned
when not needed. These are examples, not addresses to apply to an existing site.
All three objects use the implemented one-bit behavior; consult the
[KNX design reference](actuator-knx-design.md) for DPTs and flags.

Applying web commissioning changes configuration. Resolve ETS/web ownership,
read the current revision, validate mappings and then apply under your agreed
commissioning plan. Read back the committed snapshot. Only a controlled group
write plus independently observed contact behavior verifies switching. The live
unit's **Ready** label alone does not do that. See [address entry](knx-address-entry.md).

## Home Assistant example: discover before automating

Enable discovery only after the device and Home Assistant use the intended MQTT
broker. Expect one Edge Switch device with six relay entities and diagnostics.
Example payload on `edge_switch/<id>/relay/1/set` is the exact string `ON` or
`OFF`, published **without retention**. Replace `<id>` with this device's actual
identity; it is not the friendly name. Such a publish operates hardware.

First inspect availability and state, then test one approved channel. Disabled
or blocked channels should be unavailable. A reported ON is commanded state,
not measured load operation. Read [Home Assistant lifecycle](home-assistant.md)
before renaming, moving brokers, disabling discovery or removing a device.

## Xiaozhi example: begin with status

Follow [MCP setup](xiaozhi-mcp.md) using your private endpoint. Never paste the
endpoint into public docs or screenshots. After Ready, find the registered
`actuator_status_<device-identity>` tool; it accepts `{}` and reports only exposed
channels. Tool suffixes come from the device registry, not a guessed alias.

Set-relay and pulse tools are active operations. Pulse requires a `request_id`;
after a lost response, inspect state before choosing a new ID. Retry protection
is bounded and does not survive reboot. The running unit inspected for these
docs did not expose an MCP menu; this section is source-derived, not a live
Xiaozhi connectivity result.
