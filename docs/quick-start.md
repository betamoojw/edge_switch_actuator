# Your first session

Start here if your Waveshare six-channel board already has Edge Switching
Actuator firmware. If it does not, follow [build and flash](gettingstarted.md)
first. Keep connected loads isolated until installation and commissioning checks
are complete.

## Before you connect

You need the `waveshare-relay-6ch` firmware, a suitable power supply, a browser,
2.4 GHz Wi-Fi, and the unit's private setup credentials. Have the network owner
provide the intended station network. Review the [hardware and wiring guide](esp32-s3-relay-6ch-hardware.md)
and record the board revision and firmware image identity.

For an already commissioned unit, use its assigned station address and account.
Do not reset it merely to follow these instructions: reset erases configuration.

## Connect and sign in

1. Power the board using its supported supply connection. Obtain the unique
   **Device setup password** from its existing private setup label or local
   serial console at 115200 baud. Never attach that label or console capture to
   a public support issue.
2. For a fresh unit without an uplink, join `ESP32-SvelteKit-<unique_id>` using
   that password and open `http://192.168.4.1`.
3. Sign in as `admin` with the unit's setup password. Existing configured users
   retain their saved credentials. There is no shared actuator factory password.
4. Open **WiFi → WiFi Station**, enter the intended network details, and apply.
   This step changes the network; expect to reconnect your browser at the station
   IP assigned by the network. Find it in the router's client list or locally
   on the device's network status page.
5. Open **Switching Actuator**. Expect six channel cards, a network summary,
   protocol status and uptime. Fresh source defaults are protocol Off and all
   outputs OFF. Saved units can differ.

These are commissioning instructions, not actions performed during the
[read-only documentation inspection](live-verification.md).

## Read the dashboard before commanding anything

Follow the [annotated interface tour](interface-tour.md). An **OFF** badge means
the firmware commands that output off; it does not prove an isolated circuit or
open contact. The `startup` text below a channel is its last command source,
not a promise that startup ON is configured.

Review names, enabled channels, startup policy, pulse duration, disconnect-off
policy and click bindings. In commissioned KNX mode, use the KNX application
parameters as the authoritative relay configuration. Confirm the assigned user
role and channel permissions with the installer.

## Choose a control path

Use the browser alone with fieldbus Off, or choose one of RTU, TCP and KNX/IP.
MQTT/Home Assistant and Xiaozhi MCP are independent, optional integrations.
The [integration chooser](integrations.md) explains prerequisites and first reads.
Enabling an integration can introduce another command source; coordinate its
ownership with the installation operator.

## First controlled output test

Do this only with the installation owner's authorization and a prepared,
isolated low-voltage fixture. The documentation session did **not** run it.

1. Identify one enabled, unblocked channel and its permitted test load. Keep
   other automation command sources inactive under the agreed test plan.
2. Record the initial state and configured pulse duration.
3. Issue **Turn ON** for that channel. Expect its badge to become ON; independently
   check the fixture/contact response.
4. Issue **Turn OFF**, verify both UI and fixture, then issue one **Pulse**.
   Expect ON followed by OFF after its saved duration.
5. Record results and restore the agreed output state. Stop on unexpected
   activity; do not use a bulk ON command as a connectivity test.

See [commissioning acceptance](commissioning.md) for restart, fieldbus, failure
and recovery tests. Before handover, give each operator an appropriate account,
retain a private configuration record, and document which channels control which
loads. Use [troubleshooting](troubleshooting.md) if an expected result is missing.
