# Contract authority

Source baseline: `dev` fd848362c0070957ca9db165865dfa23442d1ae8.

Executable source-derived vectors are in `../simulator/contract.test.mjs`; they
exercise the actual simulator HTTP/WebSocket server, not fetch interception.

| Cases                                                          | Firmware authority                                              |
| -------------------------------------------------------------- | --------------------------------------------------------------- |
| Config bounds, six relays, three bindings, enum indices        | src/device/DeviceConfig.cpp                                     |
| Commands, capabilities, deduplication, error bodies            | src/device/ActuatorApi.cpp                                      |
| Startup, watchdog, protocol rollback, pulse/indicator expiry   | src/device/Actuator.cpp                                         |
| KNX takeover, revision, group mapping and parameter projection | src/protocols/KnxAdapter.cpp                                    |
| JWT boot/age/rotation and user validation/redaction            | lib/framework/SecuritySettingsService.{h,cpp}                   |
| Settings response envelopes                                    | lib/framework/HttpEndpoint.h and individual \*SettingsService.h |
| Event codecs, subscriptions and origin exclusion               | lib/framework/EventSocket.cpp and EventEndpoint.h               |
| Upload field, MD5, image header and HTTP/OTA errors            | lib/framework/UploadFirmwareService.cpp                         |

No captured hardware results are checked in or claimed. `npm run contract:capture
-- <output.json>` captures read-only routes from DEVICE_HOST with credentials in
DEVICE_USERNAME/DEVICE_PASSWORD. Run separately against matching simulator and
firmware profiles, review omitted fields/status/content-type/shape, and explicitly
allow volatile measurements and identity differences. Do not normalize revision,
capabilities, error codes or protocol enum differences. The capture is an aid to
review, not an automatic proof of conformance; protocol interface and mutation comparisons
require an isolated physical device. Never commit credentials or unreviewed traces.
