# Current release and documentation baseline

Reviewed on **11 October 2026** at `dev` commit
[`bf8cb12`](https://github.com/betamoojw/edge_switch_actuator/tree/bf8cb12ab9ada4337751968332c7d5e17e056ccc).
`platformio.ini` sets `APP_VERSION` to **0.6.4**. This page describes source changes;
it does not assert that a connected device runs this image.

## Firmware 0.6.4

- The shared GitHub repository identifier is now `betamoojw/edge_switch_actuator`.
  Removing `/tree/dev` fixes the release API URL constructed by the update UI.
- The sidebar replaces the Discord link with **Project website**, pointing to
  this GitHub Pages documentation.
- Generated embedded UI and checked-in default-board firmware artifacts were
  refreshed. See [image selection](buildprocess.md#firmware-build-and-release-artifacts).
- `buildRelease/Edge_S3_Relay_6CH.knxprod` is now tracked alongside firmware
  artifacts. Its presence does not prove ETS acceptance or KNX certification,
  and the normal firmware packaging hook does not generate it.

Compared with the previous documentation baseline `ced3e6d`, there are no changes
to backend REST routes, relay policies, persisted configuration schemas, Modbus,
KNX, MQTT or MCP behavior. Existing behavior is explained in the relevant guides.

## Remaining limits and evidence

The release picker still matches assets by `.bin` and a board-name substring;
merged images and related build profiles can match. Use the exact manual OTA
image until selection is made precise. A corrected URL is not proof of safe
artifact selection or of a successful live update.

The English screenshots under `media/live/` were captured on **0.6.3** on
10 October 2026. Their sidebar is outdated for 0.6.4. The user-supplied KNX images
have an unverified firmware version. No relay, indicator, reboot, reset, firmware
update or configuration change was performed for this documentation update.

[Current source review](actuator-source-review.md) lists unresolved findings.
[Earlier validation](validation.md) retains dated software and device evidence;
those checks are not represented as new 0.6.4 qualification results.

## Documentation validation — 11 October 2026 { #documentation-validation }

All **32 current guides** are available in English, Simplified Chinese and
Traditional Chinese (96 localized pages). Historical records retain their
original language and are visibly marked. Both Chinese editions were reviewed
for regional terminology and technical meaning; this was an agent review, not
an independent human translation certification.

- Strict MkDocs build passed. The offline checker validated current-page coverage,
  generated links and anchors, image alternative text, selectors and indexed
  content across 166 HTML pages.
- Browser checks at 1440 × 1000 and 390 × 844 covered desktop/mobile layout,
  navigation, equivalent-page language switching, translated diagrams and images.
  Searches for `relay`, `继电器` and `繼電器` returned results. Search uses a shared
  multilingual index, so technical terms can match several language editions.
- Existing device screenshots and KNX images were reviewed for privacy; private
  endpoints in the KNX images were masked. Text examples use documentation
  placeholders; the documented factory AP address remains unchanged.
- No new device session, relay test, ETS download, firmware build, OTA update or
  hardware qualification was performed. The existing 0.6.3 captures and unknown-
  version KNX images retain their evidence limits.

The Pages workflow repeats the strict build and offline checks before deployment.
Each language's `build-info.json` identifies the deployed documentation commit.
