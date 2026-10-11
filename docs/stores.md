# Stores

## User

The user store holds the current users credentials, if the security features are enabled. Just import it as you would use with any svelte store:

```ts
import { user } from "$lib/stores/user";
```

You can subscribe to it like to any other store with `$user` and it has the following properties:

| Property             | Type      | Description                                       |
| -------------------- | --------- | ------------------------------------------------- |
| `$user.bearer_token` | `String`  | The JWT token to authorize a user at the back end |
| `$user.username`     | `String`  | Username of the current user                      |
| `$user.admin`        | `Boolean` | `true` if the current user has admin privileges   |

In addition to the properties it provides two methods for initializing the user credentials and to invalidate them. `user.init()` takes a valid JWT toke as an argument and extracts the user privileges and username from it. `user.invalidate()` invalidates the user credentials and redirects to the login pages

!!! warning "User credentials are stored in the browsers local storage"

    The JWT is stored in browser local storage and is accessible to scripts executing in the same origin. Prevent script injection and keep management on a controlled network. The default device HTTP connection does not encrypt credentials or tokens in transit. Do not infer a scheduled security fix from this limitation.

## Event Socket

The `led` example below belongs to the generic framework demo. The actuator
subscribes to read-only `device.state` and uses REST for mutations; sending a
`device.state` event does not operate its relays.

The [Event Socket System](statefulservice.md#event-socket) is conveniently provided as a Svelte store. Import the store, subscribe to the data interested with `socket.on`. To unsubscribe simply call `socket.off`. Data can be sent to the ESP32 by calling `socket.sendEvent`

```ts
import { socket } from "$lib/stores/socket";

let lightState: LightState = { led_on: false };

onMount(() => {
  socket.on<LightState>("led", (data) => {
    lightState = data;
  });
});

onDestroy(() => socket.off("led"));

socket.sendEvent("led", lightState);
```

Subscribing to an invalid event will only create a warning in the ESP_LOG on the serial console of the ESP32.

## Telemetry

The telemetry store can be used to update telemetry data like RSSI via the [Event Socket](statefulservice.md#event-socket) system.

```ts
import { telemetry } from "$lib/stores/telemetry";
```

It exposes the following properties you can subscribe to:

| Property                           | Type      | Description                                 |
| ---------------------------------- | --------- | ------------------------------------------- |
| `$telemetry.rssi.rssi`             | `Number`  | The RSSI signal strength of the WiFi in dBm |
| `$telemetry.rssi.ssid`             | `String`  | Name of the connected WiFi station          |
| `$telemetry.rssi.disconnected`     | `Boolean` | True when Wi-Fi RSSI is unavailable         |
| `$telemetry.battery.soc`           | `Number`  | Battery state of charge                     |
| `$telemetry.battery.charging`      | `Boolean` | Is battery connected to charger             |
| `$telemetry.ota_status.status`     | `String`  | Status of OTA                               |
| `$telemetry.ota_status.progress`   | `Number`  | Progress of OTA                             |
| `$telemetry.ota_status.error`      | `String`  | Error message of OTA                        |
| `$telemetry.ethernet.connected`    | `Boolean` | Connection status of the ethernet interface |

## Analytics

The analytics store holds a log of heap and other debug information via the [Event Socket](statefulservice.md#event-socket) system.

```ts
import { analytics } from "$lib/stores/analytics";
```

Each analytics property is a `number[]`, not a scalar. Heap, filesystem and
PSRAM values are divided by 1000 in the store (decimal kB); temperature and uptime
retain their incoming units. Additional arrays include `used_heap`, `total_heap`,
`free_psram`, `used_psram` and `psram_size`.

| Property                    | Type     | Description                                    |
| --------------------------- | -------- | ---------------------------------------------- |
| `$analytics.uptime`         | `number[]` | Uptime of the chip in seconds since last reset |
| `$analytics.free_heap`      | `number[]` | Free heap in decimal kB                              |
| `$analytics.min_free_heap`  | `number[]` | Minimum free heap in decimal kB                |
| `$analytics.max_alloc_heap` | `number[]` | Largest free contiguous block in decimal kB           |
| `$analytics.fs_used`        | `number[]` | Filesystem use in decimal kB                  |
| `$analytics.fs_total`       | `number[]` | Filesystem capacity in decimal kB                 |
| `$analytics.core_temp`      | `number[]` | Core temperature (on some chips)               |

By default there is one data point every 2 seconds. It holds 1000 data points worth roughly 33 Minutes of data.
