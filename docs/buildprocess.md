# Build and firmware updates

## Toolchain and targets

Use Python 3.11+ and Node.js 24. Install `requirements-dev.txt` and run `npm ci`
in `interface/`; see [getting started](gettingstarted.md). `platformio.ini`
pins the pioarduino platform to `55.03.312-1`, uses Arduino, and isolates its core
under `.pio/network-platformio`. Firmware `APP_VERSION` is currently `0.6.4`.

| Environment | Application / purpose |
| --- | --- |
| `waveshare-relay-6ch` | Default six-relay actuator; ESP32-S3, 8 MB partition table |
| `waveshare-relay-6ch-mcp-off` | Compile regression with MCP excluded |
| `waveshare-relay-6ch-mcp-no-mqtt` | Compile regression proving MCP does not require MQTT |
| `esp32-s3-devkitc-1`, `esp32-c3-devkitm-1`, `esp32dev` | Generic framework/light demo profiles |
| `Kincony-B16M`, `esp32-wt32-eth01` | Framework profiles with Ethernet enabled |

The KNX dependency is pinned to commit
`980c047ad7fc5e27bf2fae95e48acde5d5e0b4fd`; WebSockets is pinned to 2.7.2.
Other dependencies include version ranges, so the entire firmware dependency
set is not a complete reproducible lock. The frontend uses `package-lock.json`.

## Selecting features

The effective actuator flags combine `features.ini`, common flags and profile
overrides. Security, MQTT, NTP, manual/download OTA, analytics, core dump and MCP
are compiled in. Sleep and battery are disabled; Ethernet is not enabled for the
default Waveshare profile. MQTT and MCP still start disabled at runtime.

MCP requires security and NTP and forbids `SERVE_CONFIG_FILES`. Keep credentials
out of source/build flags. Device factory admin/AP credentials come from
`SetupIdentity`, overriding the generic template values; see
[device credentials](device-credentials.md).

## Build flow

```sh
pio run -e waveshare-relay-6ch
python scripts/check_firmware_size.py waveshare-relay-6ch
```

1. `scripts/build_interface.py` builds and compresses the UI when its change check
   requires it. With default `EMBED_WWW`, assets become `lib/framework/WWWData.h`.
2. `scripts/generate_cert_bundle.py` prepares the configured Adafruit CA bundle.
3. PlatformIO compiles `.pio/build/<environment>/firmware.bin` and `firmware.elf`.
4. Existing hooks archive symbols in `build/elf/`, merged images in `build/merged/`,
   and OTA files in `build/release/`.
5. `scripts/package_release.py` packages the current outputs into `buildRelease/`,
   including on incremental normal builds. Filesystem-only/clean/erase targets
   do not create release packages.

The embedded-UI freshness check uses timestamps under `interface/src/`. Changes
only to static assets, dependencies or Vite configuration can be missed. For those
changes, remove the generated `lib/framework/WWWData.h` before building to force
regeneration. See the [source review](actuator-source-review.md).

The size checker compares the actual application binary to generated application
slots. A successful linker estimate alone is insufficient, especially on the
constrained `esp32dev` and `esp32-wt32-eth01` profiles, which use LTO.

## Firmware build and release artifacts

For the default board and version 0.6.4:

| File in `buildRelease/` | Use |
| --- | --- |
| `edge_switch_actuator_waveshare-relay-6ch_0.6.4_ota.bin` | Application-only OTA image |
| `edge_switch_actuator_waveshare-relay-6ch_0.6.4_ota.md5` | Plain hexadecimal MD5; optional first upload in update UI |
| `edge_switch_actuator_waveshare-relay-6ch_0.6.4_webflash.bin` | Merged bootloader/partition/boot application/firmware image, initial flash at offset `0x0` |
| `edge_switch_actuator_waveshare-relay-6ch_0.6.4.elf` | Matching debug symbols |

Names use the effective `APP_VERSION` and PlatformIO environment. Rebuilding the
same pair replaces its files; other versions are retained. Generation is staged
before replacing files, and a merge failure fails the build. Packaging tests
exercise this behavior. At the reviewed baseline, four default-board 0.6.4 files
in `buildRelease/` are tracked in Git. The directory also contains
`Edge_S3_Relay_6CH.knxprod` for KNX commissioning; the firmware packaging hook
does not generate that package automatically. Verify it separately.
This directory is tracked; this directory is **not** ignored.

Use **`_ota.bin` for OTA**, never `_webflash.bin`. With `EMBED_WWW`, the application
and UI update together without deliberately replacing filesystem settings.
Removing `EMBED_WWW` switches UI delivery to LittleFS: `buildfs` / `uploadfs` are
separate operations, and the release package does not contain that filesystem
image. A filesystem upload can replace stored settings; plan backups accordingly.

## Updating a device

An administrator can upload the matching OTA image under **System → Firmware
Update**. Confirm the board/environment and preserve matching debug symbols.
The MD5 file detects accidental corruption; it is not a firmware signature.
The build pipeline does not establish signed-image authenticity.

Firmware 0.6.4 fixes the GitHub repository identifier used by the release picker:
`page.data.github` is now `betamoojw/edge_switch_actuator`, without `/tree/dev`.
The sidebar also links directly to this documentation site. These are source-verified
changes, not evidence of a live OTA test.

Asset matching still checks `.bin` and a board-name substring, so merged images
and alternate MCP profiles can also match. Prefer a verified, matching manual
`_ota.bin` and do not publish ambiguous assets to a release consumed by this picker.

Firmware CI retains `buildRelease/` artifacts for 14 days; it does not create tags
or GitHub Releases. Publishing this documentation does not flash a device or
publish a firmware release.

## Certificates and factory settings

`board_ssl_cert_source = adafruit` and `src/certs/x509_crt_bundle.bin` configure the
embedded trust bundle. MCP checks WSS hostname/certificate trust and waits for
plausible time. `DOWNLOAD_OTA_SKIP_CERT_VERIFY` is commented out in the reviewed
profile; there is no reason to enable an insecure download bypass for routine use.

Factory values are in `factory_settings.ini`. They are defaults for unset values,
not a migration mechanism for commissioned units. Verify NTP's label and POSIX
zone together: the current `Europe/Berlin` label is paired with a UK-style format,
which is recorded as a review finding. Use **Connections → NTP** to select a
matching zone after provisioning.

For KNX artifact generation, see the
[package README](https://github.com/betamoojw/edge_switch_actuator/blob/dev/knx/README.md).
For docs builds and publication, see [documentation publishing](documentation.md).
