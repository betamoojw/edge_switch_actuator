# dev source review

Implementation follow-up: [implemented behavior, build steps and qualification status](actuator-implementation.md).
Review baseline: local `dev`, commit `81827a2af9410b91741ece1a23cf6ee95877b757`, 2026-09-26. Working tree was clean before these documents. No remote refresh, firmware build, hardware test or ETS test was performed. This is a source/design review, not a comprehensive security audit.

## Existing architecture to retain

`src/main.cpp` composes `ESP32SvelteKit`, PsychicHttp and the light/MQTT demonstration. `lib/framework/ESP32SvelteKit.cpp` owns Wi-Fi/AP, security, events, settings, maintenance and its own FreeRTOS loop. The Arduino loop deletes itself. Keep that composition and introduce dedicated application tasks rather than putting time-sensitive work into the framework networking loop.

`StatefulService<T>` supplies synchronized state access and change propagation. `HttpEndpoint`, `EventEndpoint`, `WebSocketServer` and `MqttEndpoint` bridge transports; `FSPersistence` stores configuration. Preserve these abstractions, with narrow extensions for permissions, validation and durable commits. State locks do not make multi-service changes transactional, and update callbacks execute after the state lock is released.

The interface uses Svelte 5, SvelteKit static output, Tailwind and DaisyUI. Existing settings cards, menus, user store and MessagePack event store are suitable foundations. Reuse Wi-Fi, user administration, metrics and OTA screens; replace the demo navigation with actuator pages.

The only application hardware service is `LightStateService`: one boolean, generic `LED_BUILTIN`, direct GPIO writes and Home Assistant MQTT integration. There are no six-relay, RGB-pattern, buzzer, button-gesture, Modbus or KNX services in the inspected application. Production support is proposed in [the design](actuator-production-design.md), not implemented by this documentation change.

## Findings requiring production changes

| Priority | Evidence | Impact and required change |
| --- | --- | --- |
| P1 | `lib/framework/ESP32SvelteKit.cpp`, `_loop()`: `wifi_eth_combined` starts outside the loop and is only set true inside it | Once connected, the reported aggregate connection can remain connected after disconnect. Recompute every iteration and use Wi-Fi events/IP state for indicator transitions. Test connect/disconnect/reconnect without MQTT. |
| P1 | `EventEndpoint.h:56`, `EventSocket.cpp:83–100`; socket admission in `ESP32SvelteKit.cpp:30` | Authenticated socket clients can invoke registered state updaters without a per-operation role check. Do not reuse writable demo events for administrative settings. Retain authenticated identity per connection and authorize subscriptions and commands, or make new events read-only and use authorized REST commands. |
| P1 | `platformio.ini:43`, `DownloadFirmwareService.cpp:117–127` | Download OTA selects `setInsecure()`. Remove bypass in the production profile, enforce trusted downloads and image authenticity/board compatibility before activation. |
| P1 | `factory_settings.ini:18,28,30`; `SecuritySettingsService.h`, `read()`; `SecuritySettingsService.cpp:102–121`; HTTP listener in `ESP32SvelteKit.cpp:81` | Shared credentials, plaintext stored/exported passwords, non-expiring token payload and HTTP management are unsuitable production defaults. Provision unique credentials, hash user passwords, separate private persistence from redacted API serialization, introduce expiry/revocation and protect management transport. |
| P1 | `EventSocket.cpp:83,93,163–170` | Subscription mutation is not consistently locked; removal from the same list inside a range-for can invalidate the iterator. Lock consistently, deduplicate/bound subscriptions, and erase with a safe iterator or snapshot. Exercise disconnect/broadcast races. |
| P1 | `FSPersistence.h:75`; `ESP32SvelteKit.cpp:70` | Writes truncate in place; mount enables format-on-failure. A power failure or mount fault can discard commissioned configuration. Use versioned, verified generations and a recoverable manifest; do not silently format production configuration. |
| P1 | `FactoryResetService.cpp:42–57` | Reset deletes flat `/config` files then immediately restarts. It neither coordinates output shutdown nor clears future KNX/NVS storage. Introduce reset participants and a durable reset-in-progress marker. |
| P2 | `LightStateService.cpp:46,72`; `src/main.cpp:50` | Generic LED writes cannot drive board RGB; no application loop remains to poll buttons or protocols. Add board drivers and explicit application task ownership. |
| P2 | `SecurityManager.h`, `User`; `HttpEndpoint.h`, `begin()` | One admin flag and one shared GET/POST predicate cannot express viewer/operator/installer permissions. Add capability predicates and separate read/write authorization. Enforce before state mutation, not in an after-update hook. |
| P2 | `features.ini:8`; `factory_settings.ini`, wake pin 0 | Deep sleep conflicts with an always-available actuator and BOOT ownership. Disable sleep in the product profile. |
| P2 | `platformio.ini`, S3 environment and dependency ranges | Generic board settings and floating library ranges are not a reproducible qualified target. Add a Waveshare environment, verified partition map and pinned dependencies. Count generated routes against server capacity. |

Line references above identify the reviewed commit; use the named methods if later edits shift them. These are static findings. In particular, no remote exploit or electrical failure was reproduced.

## Integration map

| Existing location | Planned extension |
| --- | --- |
| `src/main.cpp` | Initialize board safe state, framework, settings, controller and protocol supervisor |
| `src/Light*` | Retire from production composition; retain as framework examples if useful |
| `src/board/`, `src/device/`, `src/protocols/` (new) | Hardware drivers, canonical model, protocol adapters |
| `lib/framework/Security*`, `HttpEndpoint.h`, `EventSocket*` | Capability checks, read/write predicates, authenticated event identities |
| `lib/framework/FSPersistence.h`, `FactoryResetService*` | Atomic configuration lifecycle and reset participants |
| `interface/src/lib/types/models.ts`, stores | Typed config/status/commands, revisions, reconnect snapshots |
| `interface/src/routes/menu.svelte`, new hardware/protocol routes | Capability-filtered navigation and controls |
| `platformio.ini`, `features.ini`, build scripts | Product environment, release checks, KNX artifact generation |

Implementation and validation sequencing is specified in the production design.
