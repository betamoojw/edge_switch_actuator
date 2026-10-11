# Troubleshooting and recovery

Start with observations: firmware version, uptime, selected protocol, fault,
channel state, and the last action. Record the exact image hash when known.
Keep credentials, raw core dumps and configuration exports private. See
[live inspection](live-verification.md) for the limits of UI-only evidence.

| Symptom | Check first | Next step |
| --- | --- | --- |
| Cannot reach the device | Power, correct network, router-assigned IP, AP versus station connection | Use the provisioning address only while connected to its AP; do not reset a commissioned unit as a first step |
| Login rejected | Correct device/account; unique setup password versus saved account password | Follow [credential recovery](device-credentials.md); factory reset erases configuration |
| ON control unavailable or rejected | Role/channel mask, enabled and blocked state | Ask the installer to inspect policy; do not bypass blocks with another transport |
| UI says ON but load is off | UI reports commanded output only | Isolate and have a qualified installer verify wiring/contact/load; never treat the badge as proof of safe isolation |
| Pulse ends unexpectedly | Saved duration, later commands, channel disable and disconnect policy | Compare activity and all active command sources; commands can cancel a pulse |
| `waiting_network` | Selected uplink has usable IPv4 | AP-only connectivity does not satisfy TCP/KNX/MCP requirements |
| RTU timeout | Unit ID, baud/parity, A/B labeling, termination, selected mode | Start with reads; inspect CRC/frame counters and master wiring |
| TCP timeout | Selected mode, uplink address, port, peer restriction and firewall | Verify routing and client count; idle sessions close after 60 seconds |
| KNX edit conflicts | Current KNX revision and owner | Reload snapshot and reconcile edits; do not blindly overwrite an ETS download |
| KNX page reports HTTP 401 and disables controls | Browser authentication is no longer accepted | Sign in again and reload the snapshot before editing; see the [web takeover screenshot](knx-address-entry.md#taking-over-for-web-editing) |
| HA entities missing/stale | MQTT enabled/connected, HA discovery, prefix and broker ACLs | Check retained discovery cleanup and HA birth topic in the [HA guide](home-assistant.md) |
| MCP missing from navigation | Running build and embedded UI feature availability | Compare image identity to current source; version 0.6.3 alone is insufficient |
| MCP waiting for time / retrying | NTP, DNS, uplink and endpoint validity | Use the [MCP error guide](xiaozhi-mcp.md#runtime-and-troubleshooting); do not disable TLS verification |
| GitHub update list fails | Running version, GitHub connectivity/rate limits and available release assets; the repository URL is fixed in 0.6.4 | Use a verified matching manual `_ota.bin`; see [updates](buildprocess.md#updating-a-device) |
| RGB test not visible / no tone | Enabled state and indicator priority | Fault/programming/reset indications take priority; buzzer must be enabled |

## Configuration and recovery boundaries

Actuator, KNX and MCP settings use verified storage generations. Generic network,
user and integration framework settings still use an in-place JSON persistence
path; power-loss resilience differs. This is a source finding, not a completed
power-interruption qualification. Keep a private installation record of network,
users, relay policies, integrations and KNX associations.

Manual OTA is intended to preserve filesystem configuration with embedded UI.
It is not a backup mechanism. A filesystem upload, full erase, incompatible
downgrade or factory reset can destroy settings. Preserve image identity and
matching ELF symbols and plan a local recovery path before updating.

Factory reset intentionally removes user settings and KNX commissioning while
retaining the manufacturing setup identity. The reserved ten-second BOOT hold
also resets configuration; disabling click actions does not disable that hold.
Reprovision after reset using the private per-device password. If firmware is
unbootable, follow the board's supported USB downloader procedure and
[initial flashing guide](gettingstarted.md#3-build-and-flash-a-unit), with loads
isolated. Reflashing is an active maintenance operation, not a diagnostic read.

## Useful support report

Include board revision, firmware version/hash, browser version, selected protocol,
redacted screenshot, expected versus actual behavior, reproducible steps and
whether it occurs in the simulator. Include fault/counter values and timestamps,
but remove network credentials, tokens, QR setup labels, endpoint query strings
and unnecessary device identifiers. Link to the relevant source finding when
reporting a known issue instead of assuming a new hardware fault.
