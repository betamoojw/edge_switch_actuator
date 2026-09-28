# Arduino-ESP32 Network API migration

Reviewed local `dev`, 2026-09-27. The application previously used Wi-Fi connectivity as a prerequisite for MQTT, Modbus TCP and KNX, while framework connection status separately combined Wi-Fi and Ethernet. NTP subscribed to individual interface disconnects, and the KNX platform inherited compile-time Wi-Fi addressing.

## Core and tooling

The platform is pinned to pioarduino `55.03.312-1`, packaging official Espressif Arduino-ESP32 **3.3.12** / ESP-IDF 5.5.5. The platform requires PlatformIO **6.2.0**. Install `requirements-dev.txt` into a virtual environment and run its `platformio` executable. Project `core_dir` is `.pio/network-platformio`, isolating the platform installer's managed Python and toolchains from the global PlatformIO installation. This directory and downloaded packages are ignored by Git.

Sources: [official Network API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/network.html), [official 3.3.12 release](https://github.com/espressif/arduino-esp32/releases/tag/3.3.12), [PlatformIO packaging release](https://github.com/pioarduino/platform-espressif32/releases/tag/55.03.312-1).

Reproduce the firmware validation from a virtual environment containing `requirements-dev.txt`:

```shell
platformio run -e waveshare-relay-6ch -e esp32-s3-devkitc-1 -e Kincony-B16M
python scripts/test_native.py
```

Native tests also require a C++17 compiler, selected through `CXX`, an available `clang++`/`g++`, or the local Zig installation.

## Implemented behavior

`Network.begin()` initializes shared networking before interface startup. All application event subscriptions use `Network.onEvent` and `arduino_event_id_t` / `arduino_event_info_t`. Wi-Fi station IP configuration and address access use the `NetworkInterface` inherited by `WiFi.STA`.

`NetworkSupport` provides one view of the core-selected default **IPv4 uplink**, requiring connected link and assigned IP. It excludes the provisioning AP even if the core temporarily makes AP the default route. Default-route selection remains the core's responsibility; no second application routing policy competes with it. This is local network readiness, not an Internet-reachability probe. The current protocol interface protocols remain IPv4; an IPv6-only uplink does not open them.

- Framework connection status uses the shared uplink. `NETWORK`, `NETWORK_CONNECTED` and `NETWORK_MQTT` replace station-specific internal names; old `STA` enum aliases and numeric values remain compatible.
- MQTT and NTP reconcile the selected interface index and IPv4 address from the framework loop. Losing a nonselected interface no longer independently tears down a healthy uplink. A route change, including a different interface with the same IP, reconfigures clients.
- Provisioning AP fallback uses uplink readiness, so Ethernet connectivity can suppress the fallback AP. Recovery mode and always-on AP behavior remain available.
- OTA uses `NetworkClientSecure`; Modbus TCP uses `NetworkServer` / `NetworkClient` and binds the selected uplink address. Actuator listeners restart when either the address or selected interface changes.
- KNX overrides all inherited network address, MAC, multicast and unicast methods with `NetworkInterface` / `NetworkUDP` implementations. The pinned third-party stack is not modified. Route changes close and reopen KNX multicast. UDP bind/join/send failures are propagated. KNX requires a multicast-capable LAN; a generic PPP uplink may be suitable for MQTT but unsuitable for KNX routing.
- Device status adds `networkOnline` and `networkInterface`; `ip` is the selected uplink address. `sta` remains actual Wi-Fi station readiness for compatibility. Device dashboard labels and RGB/buzzer connectivity indications use generic network readiness. System status also exposes a `network` object.
- Modbus network-ready bits/state now describe the active uplink; register offsets and numeric state codes are unchanged.

## Intentionally interface-specific

Wi-Fi credentials, scanning, RSSI, AP clients, radio power and station/AP start/stop still use Wi-Fi driver APIs. Ethernet PHY/SPI initialization, link speed and interface configuration still use `ETH`, which implements `NetworkInterface`. `Network` does not replace hardware setup. Existing interface-specific dashboard diagnostics remain. No Ethernet hardware or PPP modem provisioning is invented for the Waveshare board; any additional uplink needs correctly configured physical hardware.

## Validation

`python scripts/test_native.py` includes host tests of the actual shared uplink helper against a small fake core API: no interface, pre-DHCP association, AP-only boot, stale IP after link loss, Ethernet surviving unrelated station loss, same-IP interface change, and PPP selection. These tests complement the existing gesture, Modbus and product-contract tests.

Validation completed on 2026-09-27:

- Firmware builds passed on Arduino-ESP32 3.3.12 for `waveshare-relay-6ch`, `esp32-s3-devkitc-1`, and Ethernet-enabled `Kincony-B16M`.
- Native gesture/Modbus and shared network-helper tests passed, as did all three KNX product-contract tests.
- The dashboard production build passed. A subsequent frontend cleanup resolved the 58 existing type errors and three CSS diagnostics; `npm run check` now reports zero errors and zero warnings. The `svelte-focus-trap` metadata warning was subsequently fixed with a local package containing unchanged upstream source and corrected conditional exports; checking and production builds pass without that warning.
- Python lint and Git whitespace checks passed. Firmware artifact hashes are recorded in [the build manifest](actuator-build-manifest.json).

No hardware was flashed. Physical cable/WLAN failover, DHCP renewal, MQTT reconnection, NTP recovery, TLS OTA and ETS multicast commissioning still require device testing.
