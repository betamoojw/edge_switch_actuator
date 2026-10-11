# Framework services

This reference describes the current extension APIs in `lib/framework/` at firmware
0.6.4. The framework derives from [ESP32-SvelteKit](https://github.com/theelims/ESP32-sveltekit).
The [retained legacy tutorial](framework-tutorial-legacy.md) preserves the original
light-demo examples; some signatures and examples there are obsolete. For actuator
ownership and public contracts, use [architecture](architecture.md) and [REST](restfulapi.md).

## Initializing the framework

`src/main.cpp` constructs `PsychicHttpServer` and `ESP32SvelteKit(&server, 160)`
for the actuator, sets safe GPIO and loads setup identity before startup.
`esp32sveltekit.begin()` controls server startup order; do not start the HTTP server
independently. Count each registered method and generated static route when sizing
endpoint capacity. `HttpEndpoint<T>` adds GET/POST and optionally CORS OPTIONS.
Do not reuse historical demo endpoint counts as a current capacity calculation.

## Stateful Service

`StatefulService<T>` owns state behind a FreeRTOS recursive mutex. `read(callback)`
and `update(callback, originId)` protect access. Updates return
`StateUpdateResult::CHANGED`, `UNCHANGED` or `ERROR`. Only CHANGED propagates update
notifications. `updateWithoutPropagation` skips hooks and update notifications;
callers must explicitly perform any required commit/notification work.

### Update Handler

`addUpdateHandler(callback, allowRemove = true)` returns an ID for
`removeUpdateHandler(id)`. The callback receives `const String &originId`.
Use origins to prevent echo loops; common adapters use `http`, `mqtt`, socket
client IDs or `websocketserver:<clientId>`. An origin is not an authorization token.

### Hook Handler

`addHookHandler(callback, allowRemove = true)` receives
`(const String &originId, StateUpdateResult &result)` after an ordinary update,
including unchanged/error results, before CHANGED propagation. Remove with
`removeHookHandler(id)`. Hooks are not automatically called by updateWithoutPropagation.

### Read & Update State

The following example changes only demonstration memory, not actuator GPIO:

```cpp
struct DemoState { bool on = false; };
StatefulService<DemoState> demo;
demo.update([](DemoState &state) {
    if (state.on) return StateUpdateResult::UNCHANGED;
    state.on = true;
    return StateUpdateResult::CHANGED;
}, "example");
bool current = false;
demo.read([&](DemoState &state) { current = state.on; });
```

Keep hardware writes on the actuator task. A generic state mutex is not a
replacement for its command queue, per-transport permission checks or revisions.

### JSON Serialization

The current aliases are:

```cpp
// JsonStateReader<T>
void read(T &state, JsonObject &root);
// JsonStateUpdater<T>
StateUpdateResult update(JsonObject &root, T &state, const String &originId);
```

Use `service.read(root, reader)` and `service.update(root, updater, originId)`.
The updater's third argument is required; older two-argument tutorial callbacks
do not match this API. Validate type/range and redact secrets before serialization.

### HTTP RESTful Endpoint

`HttpEndpoint<T>` takes reader, updater, service, server, route, security manager
and authentication predicate (default IS_ADMIN). Call `begin()` to register it.
GET serializes current state; POST requires an object, applies without automatic
propagation, returns 400 on ERROR, calls update handlers on CHANGED and returns
the current serialized state. This abstraction does not implement the actuator's
role/channel policy or request queue; product mutations use `ActuatorApi.cpp`.

### File System Persistence

`FSPersistence<T>` connects reader/updater/service to an FS and file path.
It registers persistence on state updates; `disableUpdateHandler()` allows manual
control. Its in-place file writes are not equivalent to DurableStore's verified
generations. Do not promise interrupted-write recovery for generic settings.
See [persistence](architecture.md#persistence-and-reset).

### Event Socket Endpoint

`EventEndpoint<T>` takes reader, updater, service, shared socket and event name.
`begin()` registers the event, a write callback and initial synchronization on
subscription. It adds no per-event role authorization. Do not attach it as a
writable actuator relay endpoint; the product's `device.state` is read-only.

### WebSocket Server

`WebSocketServer<T>` is the separate generic state endpoint abstraction, with
reader/updater, service, server, path, security manager and predicate. Register
with `begin()`. Its demo path `/ws/lightState` is not the actuator event endpoint.
Check authentication and operation permissions when exposing any writable state.

### MQTT Client

When FT_MQTT is enabled, `getMqttClient()` returns the shared client.
`MqttEndpoint<T>` bridges a state service to subscribe/publish topics and
`configureBroker(subTopic, pubTopic)` changes them. The actuator instead uses
its bounded Home Assistant adapter; do not write relay GPIO from MQTT callbacks.
Broker ACLs, not browser roles, authorize MQTT. See [Home Assistant](home-assistant.md).

## Event Socket

The shared endpoint is `/ws/events`, authenticated on admission. Default encoding
is MessagePack; `EVENT_USE_JSON=1` selects JSON. Logical envelopes have `event`
and `data`, for example `{"event":"subscribe","data":"device.state"}`.
Use `unsubscribe` to leave and `ping`/`pong` for application heartbeat.

Register names with `registerEvent(name)`. `onEvent` callbacks receive
`(JsonObject &root, int originId)`; `onSubscribe` receives `const String &originId`.
`emitEvent(name, root, originId, false)` sends to subscribed clients except the
origin; `true` sends only to that origin. The default origin is an empty string.
`getConnectedClients()` exposes connection count. Frames and pending sends are bounded.
Account edits do not establish immediate revocation of existing sockets; see
[source review](actuator-source-review.md).

## Security features

Use the security manager's `wrapRequest` / `wrapCallback` with
`AuthenticationPredicates::NONE_REQUIRED`, `IS_AUTHENTICATED` or `IS_ADMIN`.
Predicates do not replace fine-grained actuator authorization. Login, boot-bound
eight-hour JWTs, redacted settings and HTTP transport limitations are documented
in [REST authentication](restfulapi.md#authentication-and-permissions).

## Placeholder substitution

Framework configuration supports `#{platform}`, `#{unique_id}` and `#{random}`
through its configured default-processing helpers. Keep placeholders literal in
templates; do not confuse them with a secret store or with the actuator's unique
NVS setup identity. Never embed real passwords in factory configuration examples.

## Accessing settings and services

`ESP32SvelteKit` exposes `getFS`, `getServer`, `getSecurityManager`, `getSocket`,
`getWiFiSettingsService`, `getAPSettingsService`, `getNotificationService`,
`getFeatureService` and `getRestartService`. Feature-gated getters include
`getSecuritySettingsService`, `getNTPSettingsService`, `getMqttSettingsService`,
`getMqttClient`, `getSleepService`, `getBatteryService` and `getXiaozhiMcpService`.
Use service read/update APIs rather than directly changing private state.
Register update handlers when consumers need to follow committed settings.

## Other functions provided

- `setMDNSAppName(name)` changes the application name used for discovery.
- `addLoopFunction(callback)` adds work to the framework task; avoid blocking it.
  The header default loop interval is 10 ms, not a real-time scheduling promise.
- `factoryReset()` is destructive; the actuator reset lifecycle drives outputs
  low and preserves the separate setup identity. `recoveryMode()` requests AP
  recovery behavior; neither is a read-only diagnostic.
- Optional `SleepService` has `sleepNow()`, `attachOnSleepCallback(callback)` and
  `setWakeUpPin(pin, level, pinTermination)`, where termination is FLOATING,
  PULL_UP or PULL_DOWN. Wake-pin defaults use `WAKEUP_PIN_NUMBER` and `WAKEUP_SIGNAL`.
  Sleep is disabled for this actuator profile; do not repurpose BOOT without
  reviewing boot straps and reset behavior.
- Optional battery APIs are `updateSOC`, `setCharging`, `isCharging`, `getSOC`.
  They do not create a battery measurement circuit; battery is disabled here.
- `getConnectionStatus()` returns OFFLINE, AP, AP_CONNECTED, NETWORK,
  NETWORK_CONNECTED or NETWORK_MQTT. STA/STA_CONNECTED/STA_MQTT are compatibility
  aliases; product uplink readiness uses NetworkSupport, not AP association alone.
- `getFeatureService()->addFeature(name, enabled)` adds a reported feature.
  Report actual capability, not a UI-only promise.

## OTA Firmware Updates

Manual upload and server download are administrator operations. Use the matching
application `_ota.bin`, never a merged flash image. Custom update servers must
preserve the download API and trust requirements. In 0.6.4 GitHub URL construction
is fixed, while broad asset matching remains unsafe for mixed images/profiles.
See [build and updates](buildprocess.md) for packaging, persistence and limitations.
