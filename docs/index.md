# Edge Switching Actuator

Control six relay outputs from a browser or an automation network. This project
combines ESP32-S3 firmware, a Svelte 5 / SvelteKit interface, and tools for
commissioning, testing and firmware packaging.

This site describes **`dev`**, reviewed at
[`ced3e6d`](https://github.com/betamoojw/edge_switch_actuator/tree/ced3e6d8588f5bc3650d31cdd54e1f80f8bc06e9)
on **10 October 2026**, with firmware version **0.6.3**. It is development-branch
documentation, not a declaration that every feature is production-qualified.

## Start here

[![Edge Switching Actuator six-channel relay enclosure with external antenna and screw terminals.](edge_switch_actuator_waveshare-relay.png)](esp32-s3-relay-6ch-hardware.md#hardware-overview)

*Project-supplied hardware image. See the [hardware guide](esp32-s3-relay-6ch-hardware.md)
for pin assignments, wiring guidance and the [dimensioned enclosure view](esp32-s3-relay-6ch-hardware.md#enclosure-dimensions).*

[Connect your first device](quick-start.md){ .md-button .md-button--primary }
[Explore the real interface](interface-tour.md){ .md-button }

![Six relay controls on the running device, all reporting OFF during read-only inspection.](media/live/outputs.jpg)

*Actual device capture, 10 October 2026. Saved settings may differ from defaults;
read the [inspection scope](live-verification.md) before treating a screenshot as
verification of physical behavior.*

| I want to… | Guide |
| --- | --- |
| Connect a pre-flashed unit | [Your first session](quick-start.md) |
| Build firmware or try it without hardware | [Developer setup](gettingstarted.md) |
| Plan wiring and handover | [Hardware](esp32-s3-relay-6ch-hardware.md) and [commissioning](commissioning.md) |
| Operate relays, indicators and button gestures | [Device operation](device-operation.md) |
| Find a unit's setup password or print its setup label | [Device credentials](device-credentials.md) |
| Connect a Modbus master | [Register map](actuator-modbus-map.md) and [verifier](modbus-verifier-quickstart.md) |
| Commission KNX addresses | [KNX commissioning](knx-address-entry.md) |
| Integrate automation services | [Home Assistant](home-assistant.md) or [Xiaozhi MCP](xiaozhi-mcp.md) |
| Understand or extend the code | [Architecture](architecture.md), [API](restfulapi.md), [frontend](structure.md) |
| Assess readiness | [Source review](actuator-source-review.md) and [validation](validation.md) |
| Diagnose a problem | [Troubleshooting and recovery](troubleshooting.md) |

## Practical uses

- **Local control panel:** give operators browser access to named channels,
  bounded pulses and current commanded state, with role and channel permissions.
- **Building automation evaluation:** integrate independent relay loads with a
  Modbus master or KNX/IP routing installation, subject to load suitability and
  commissioning. This is not a motor interlock or safety controller.
- **Home automation:** discover relay and diagnostic entities through MQTT and
  Home Assistant, with broker access controls.
- **Voice/agent integration:** expose selected channels through Xiaozhi MCP using
  a private WSS endpoint and explicit channel selection.
- **Development and bench work:** explore the UI simulator, portable protocol
  tests and a fixed hardware profile before authorizing physical output tests.

Use the [integration chooser](integrations.md) to match prerequisites to your
installation. Polished documentation does not imply hardware certification or
qualification of these use cases.

## What the product implements

The default `waveshare-relay-6ch` profile drives six relays, a BOOT button, an RGB
indicator, a buzzer and an RS485 interface. Relay configuration includes startup
state, enablement, pulse duration and disconnect policy. The interface opens at
`/device`; authentication and capabilities govern available operations.

Exactly one fieldbus mode is selected: **Off**, **Modbus RTU**, **Modbus TCP** or
**KNX/IP**. Home Assistant over MQTT and Xiaozhi MCP over WSS can operate alongside
that selection. Both integrations start disabled; MCP initially exposes no relays.

Device management includes Wi-Fi station/AP settings, NTP, users, telemetry,
crash diagnostics and OTA. [Browser preferences](ui-preferences.md) provide seven
languages and five themes. Ethernet is available in separate framework board
profiles; it is not enabled by the default Waveshare profile.

## Boundaries

- Relay status reports commanded output state; there is no contact feedback.
- A provisioning AP does not count as an uplink for TCP, KNX or outbound services.
- The device's management server uses HTTP. Keep it on a controlled network.
- The KNX product package and historical hardware checks have specific limits;
  see [validation](validation.md). Simulator success is not hardware qualification.
- Other PlatformIO boards retain template/demo code and are not interchangeable
  with the Waveshare actuator profile.

## Project and license

[Source repository](https://github.com/betamoojw/edge_switch_actuator/tree/dev).
Based on [ESP32-SvelteKit](https://github.com/theelims/ESP32-sveltekit) and its
ESP8266 React predecessor. Backend LGPL-3.0 and frontend MIT terms are recorded
in the project's [LICENSE](https://github.com/betamoojw/edge_switch_actuator/blob/dev/LICENSE).
