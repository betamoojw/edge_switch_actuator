# Getting started

The supported actuator target is `waveshare-relay-6ch`, selected by default in
`platformio.ini`. Review the [board reference](esp32-s3-relay-6ch-hardware.md)
before wiring or flashing a unit. Generic ESP32 environments build the template
application instead of the six-relay actuator.

## 1. Prepare a checkout

Use Git, Python 3.11 or later, and Node.js 24 with npm. VS Code with PlatformIO
and Svelte support is convenient but not required.

```sh
git clone --branch dev https://github.com/betamoojw/edge_switch_actuator.git
cd edge_switch_actuator
python -m venv .venv
```

Activate the environment with `.venv\Scripts\Activate.ps1` in PowerShell, or
`source .venv/bin/activate` on macOS/Linux, then install dependencies:

```sh
python -m pip install -r requirements-dev.txt
cd interface
npm ci
cd ..
```

PlatformIO installs its pinned ESP32 platform/toolchains under
`.pio/network-platformio`. The first firmware build needs network access to
download dependencies and prepare the certificate bundle.

## 2. Try the interface without hardware

```sh
cd interface
npm run dev:sim
```

Open the local URL printed by the runner. Sign in with `admin` / `sim-admin`.
These credentials are local fixtures only. The simulator does not connect to
physical relays, MQTT brokers, KNX networks or Xiaozhi. See
[frontend development and testing](frontend-testing.md) for additional roles,
test profiles and real-device proxy mode.

## 3. Build and flash a unit

From the repository root:

```sh
pio run -e waveshare-relay-6ch
python scripts/check_firmware_size.py waveshare-relay-6ch
```

The build embeds the compressed UI and produces versioned files in
`buildRelease/`. A connected board can be flashed with:

```sh
pio run -e waveshare-relay-6ch -t upload
pio device monitor -b 115200
```

Specify `--upload-port <port>` if several serial devices are attached. The board
profile uses an 8 MB partition layout; verify the fitted flash before use.
See [build and firmware updates](buildprocess.md) for image types and OTA.

## 4. Provision Wi-Fi and sign in

1. Read the **Device setup password** from the local serial console at 115200 baud,
   or use an existing manufacturing setup label. The firmware stores this unique
   24-character password in NVS and retains it across factory reset.
2. When no configured uplink is available, join the device AP named
   `ESP32-SvelteKit-<unique_id>` using that password.
3. Open `http://192.168.4.1`, then sign in as `admin` with the same setup password.
   Previously configured accounts keep their saved credentials.
4. Configure **WiFi → WiFi Station**, then access the device at its station IP.
5. Review **Users**, output configuration and the selected protocol before
   connecting operational loads.

The actuator profile does **not** use `admin/admin`, `guest/guest` or
`esp-sveltekit` as its factory login/AP credentials. Those constants remain for
generic template profiles. See [credential recovery and setup labels](device-credentials.md).

## 5. Select integrations

Under **Switching Actuator → Protocol Interface**, choose one fieldbus. RTU
requires RS485 enabled; TCP and KNX wait for an IPv4 uplink. Factory defaults are
all relays OFF and protocol Off. MQTT discovery and Xiaozhi MCP are separate
opt-in settings under **Connections**.

Continue with [device operation](device-operation.md), the
[Modbus register map](actuator-modbus-map.md), or [KNX address entry](knx-address-entry.md).
