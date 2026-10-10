# Live inspection record

**Date:** 10 October 2026. **Method:** authenticated browser inspection only,
using the owner's existing administrator account. Credentials and network
identity are excluded from published captures. The source reviewed is `dev`
firmware baseline `ced3e6d` (0.6.3); documentation started from `2176b76`.

## Evidence labels

| Label | Meaning |
| --- | --- |
| Observed | Visible on this running device during this inspection |
| Source-derived | Implemented behavior or defaults found in the reviewed code; not necessarily exercised on this device |
| Historical | Earlier test records with their own dates, firmware and scope |
| Unverified | No current direct evidence; remains an acceptance task |

## Observed state

| Surface | Observation | Limit |
| --- | --- | --- |
| Login and dashboard | Administrator login succeeded; six channels enabled and OFF | No non-admin account test, GPIO measurement or contact test |
| Protocol | KNX/IP running; configuration window closed | No telegram, routing or ETS test |
| KNX | Application configured, programming OFF, owner web, revision 2 | No commissioning changes made |
| KNX relay parameters | Six enabled, startup ON unchecked, OFF-on-loss unchecked, 1000 ms pulse | No restart, loss or pulse test |
| Indicators | RGB enabled at 25%, green reported; buzzer disabled, 0 Hz | UI telemetry only; no physical observation or test command |
| Button | Released; single programming, double identify, triple all-channel pulse | No button gesture performed |
| MQTT | Disabled | No broker or HA discovery test |
| Connections menu | MQTT and NTP present; MCP absent | Running build/features or embedded UI may differ from current source |
| System | Firmware 0.6.3, ESP32-S3 Rev 2, 240 MHz dual core, ESP-IDF 5.5.5 / Arduino 3.3.12 | Exact running commit/image hash unavailable |
| Flash | UI reported 16,777 KB (approximately 16 MiB) | Not a board-wide specification; source partition layout remains 8 MB |

The KNX helper's fixed triple-click wording disagrees with this unit's configured
single-click programming action. Documentation calls out that discrepancy;
firmware/UI code was not modified. A matching version string does not resolve
the absent MCP menu or prove that the device runs the reviewed commit.

## Capture provenance

Seven JPEGs in `docs/media/live/` were captured from the device: outputs,
indicators, button, protocol access, KNX, maintenance and system status. No
simulator or generated image is presented as a real-device screenshot. Cropping
only selects the relevant UI; captions provide the annotations. No settings or
telemetry values were painted over or fabricated. Each published image was
visually checked for passwords, tokens, setup QR codes and private endpoint data.

Two pre-existing, untracked product/dimension illustrations were left untouched
and excluded from the site. Their terminal labels and dimensions have not been
verified against the installed hardware revision.

## What this session did not do

No relay command, pulse, indicator/buzzer test, save/apply, configuration window,
programming toggle, reconnect, reboot, reset, firmware update or network change
was issued. Merely viewing settings is not an acceptance test of those features.

See [commissioning acceptance](commissioning.md) for concrete active procedures
requiring installation-owner authorization, [validation](validation.md) for host
and historical evidence, and [source review](actuator-source-review.md) for open
implementation findings. This documentation is not electrical qualification,
KNX certification, or a declaration of production readiness.
