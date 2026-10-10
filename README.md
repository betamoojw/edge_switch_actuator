# Edge Switching Actuator

ESP32-S3 firmware and a Svelte browser interface for a six-channel relay actuator.
The `dev` branch targets the Waveshare ESP32-S3 Relay 6CH board, with Modbus RTU,
Modbus TCP or KNXnet/IP routing, plus optional Home Assistant and Xiaozhi MCP.

**[Read the documentation](https://betamoojw.github.io/edge_switch_actuator/)** ·
[Getting started](docs/gettingstarted.md) · [Architecture](docs/architecture.md) ·
[Current source review](docs/actuator-source-review.md)

## Capabilities

- Six independent relay channels: names, enable flags, startup state, pulses,
  blocking and configurable disconnect-off behavior.
- One selected fieldbus mode: off, Modbus RTU, Modbus TCP or KNX/IP.
- Web commissioning of KNX individual/group addresses and relay parameters,
  with explicit ownership transfer from ETS.
- BOOT button gestures, RGB status/identify indication and buzzer control.
- Wi-Fi provisioning, role-based device commands, durable settings, diagnostics
  and firmware updates.
- Opt-in [Home Assistant MQTT discovery](docs/home-assistant.md) and
  [Xiaozhi MCP](docs/xiaozhi-mcp.md), independent of the selected fieldbus.
- Seven interface languages and five themes under **System → UI**.

## Build or try the interface

Use Python 3.11+ and Node.js 24. From the repository root:

```sh
python -m pip install -r requirements-dev.txt
cd interface
npm ci
cd ..
pio run -e waveshare-relay-6ch
python scripts/check_firmware_size.py waveshare-relay-6ch
```

Versioned OTA, initial-flash and debug files are packaged in `buildRelease/`.
Use the `_ota.bin` image for OTA; see [build and firmware updates](docs/buildprocess.md).

To explore the real frontend without hardware:

```sh
cd interface
npm ci
npm run dev:sim
```

The simulator uses `admin` / `sim-admin`. Physical actuator units use a unique
setup password printed on the local serial console at 115200 baud, with username
`admin`. See [credentials and labels](docs/device-credentials.md).

## Status and scope

The reviewed source baseline is `dev` commit `ced3e6d` (firmware `0.6.3`). Generic
ESP32 profiles retain framework demonstrations; they are not six-relay board
ports. Reported relay state is commanded GPIO state, not contact feedback.
Management uses HTTP; deploy on a controlled network. KNX package generation
does not establish ETS acceptance or certification.

See the [validation record](docs/validation.md) for checks performed during this
documentation refresh and the remaining hardware qualification work.

## Documentation development

```sh
python -m pip install -r requirements-docs.txt
python -m mkdocs serve
python -m mkdocs build --strict
```

Documentation changes pushed to `dev` are built and published by GitHub Actions.
See [publishing setup](docs/documentation.md).

## Attribution and license

Built on [ESP32-SvelteKit](https://github.com/theelims/ESP32-sveltekit), originally
derived from rjwats/esp8266-react. Backend code is LGPL-3.0 and frontend code is
MIT as described in [LICENSE](LICENSE); dependencies retain their own licenses.
