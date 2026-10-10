# Interface tour

These are authentic screenshots from a running Waveshare actuator, captured
read-only on **10 October 2026**. It reports firmware **0.6.3**; its exact build
commit is unknown. The screenshots show saved settings, not factory defaults.
No relay, indicator, configuration, reset or update action was performed.

Numbered annotations below each image explain the visible controls without
altering the device pixels. Crops omit network identity panels and credentials.
Open an image in a new tab to read it at full size. The
[verification record](live-verification.md) separates observations from source
behavior, including differences in this unit's navigation.

## 1. Outputs

![Outputs tab showing six OFF channel cards, individual Turn ON and Pulse controls, collapsed settings and bulk controls.](media/live/outputs.jpg)

1. **Channel title and OFF badge:** channels are numbered 1–6. All six reported
   OFF during inspection. This is commanded state; there is no contact feedback.
2. **Enabled · startup:** the channel is enabled and its last command source is
   startup. Inspect saved startup policy separately.
3. **Turn ON / Pulse:** an authorized operator can command the channel. A pulse
   uses its saved duration and then forces OFF. These controls were not pressed.
4. **Channel settings / bulk controls:** installer/admin configuration and
   all-enabled-channel commands have different permissions. A blocked or denied
   enabled channel rejects an entire web bulk command; see [roles](device-operation.md#access-roles).

## 2. Indicators

![Indicators tab with RGB enabled at 25 percent, current green RGB value, test controls, and a disabled buzzer.](media/live/indicators.jpg)

1. **RGB status:** this unit reported green `(0, 63, 0)` at 25% brightness.
   Source factory brightness is 10%; the screenshot is not the default.
2. **Test color / Identify:** these are active commands. Higher-priority fault,
   reset or programming indications can override a manual test.
3. **Buzzer:** unchecked Enabled and 0 Hz were observed. The disabled Test tone
   control agrees with that saved state. No sound or physical LED was verified.

## 3. Button

![Button tab showing single-click KNX programming, double-click identify, triple-click pulse for all channels, and the ten-second reset warning.](media/live/button.jpg)

1. **Released / event count:** software reported the button released and no
   recognized events at capture time.
2. **Click bindings and targets:** this unit uses single-click programming,
   double-click identify and triple-click pulse of all channels. Factory source
   defaults are none / identify / programming toggle. Always inspect bindings
   before pressing BOOT.
3. **Reset warning:** a ten-second hold erases settings and commissioning even
   when application click actions are disabled. Do not test this on an installed
   unit without a recovery plan.

## 4. Protocol access

![Lower Protocol panel showing write mask 63, manual indicator commands disabled, zero counters and a closed configuration window.](media/live/protocol-access.jpg)

1. **Write mask 63:** bits 0–5 permit the six relay channels for Modbus writes.
   This policy is separate from web account permissions and MCP exposure.
2. **Manual indicators and diagnostic commands:** unchecked on this unit.
3. **Counters and configuration window:** zero counters do not prove transport
   health, especially while KNX is selected. Opening a window grants temporary
   configuration access; it was left closed. RS485 masters cannot be individually
   authenticated by the RTU transport.

The upper protocol selector (outside this crop) reported **KNXnet/IP**. RTU/TCP
settings can remain visible while their transports are inactive. Only one
fieldbus runs at a time.

## 5. KNX commissioning

![KNX tab reporting application configured, programming off, web ownership, revision 2 and individual address 1.2.1.](media/live/knx.jpg)

1. **Application configured / Programming OFF:** observed software state, not
   evidence of ETS acceptance or physical telegram testing.
2. **Owner and revision:** web ownership and KNX revision 2 were visible. KNX
   has its own revision; reload after a conflicting edit.
3. **Individual address:** `1.2.1` is this unit's saved example. Assign a unique
   topology-appropriate address; do not copy it blindly. The factory hint is
   `15.15.255`.

The visible helper mentions a hardware triple-click. That wording assumes the
factory binding and is misleading for this unit's saved single-click binding.
Use the actual [Button settings](#3-button) and [KNX guide](knx-address-entry.md).
The development identity warning does not establish KNX certification.

For project-supplied views of ETS downloads and the web takeover control, see
the [ETS and web ownership walkthrough](knx-address-entry.md#ets-download-and-web-ownership-screenshot-walkthrough).
Those images are separate evidence from this read-only device capture.

## 6. Maintenance

![Maintenance tab with configuration revision, empty factory-reset confirmation field and disabled erase button.](media/live/maintenance.jpg)

1. **Revision and uptime:** useful context for support; uptime is not a durability
   or long-running reliability test.
2. **Erase confirmation:** this is destructive recovery, not a routine save or
   logout. The field was left empty and the action was not run.
3. **Recent activity:** an in-memory diagnostic view, not a durable audit archive.

## 7. System status and navigation

![System Status page with uptime, memory, filesystem, temperature, reset reason, firmware 0.6.3, ESP32-S3 and reported flash capacity.](media/live/system-status.jpg)

1. **Firmware / chip / SDK:** record these before requesting support, together
   with a firmware hash if available. A version string alone cannot identify an
   exact build or its compiled feature flags.
2. **Memory / filesystem / temperature:** point-in-time telemetry. These values
   do not establish thermal or heap qualification.
3. **System navigation:** UI preferences, status, metrics, core dump and update
   are separate pages. Firmware Update requires the matching `_ota.bin`; see
   [image selection](buildprocess.md#firmware-build-and-release-artifacts).

The observed Connections menu offered MQTT and NTP, but no Xiaozhi MCP entry.
MQTT reported Disabled. MCP and Home Assistant instructions therefore remain
source-derived for current `dev`, not end-to-end verification on this unit.
