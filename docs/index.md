---
hide:
  - navigation
  - toc
---

# Switching Actuator

Manage six ESP32-S3 relay outputs, configure the Protocol Interface for Modbus RTU/TCP or KNXnet/IP, and monitor network and device status from a desktop or mobile browser. The web interface opens Switching Actuator at `/device`, with login when security is enabled.

See [implementation and validation](actuator-implementation.md), [KNX address entry](knx-address-entry.md), and [hardware-independent frontend testing](frontend-testing.md).

Built on ESP32-SvelteKit and its ESP8266 React predecessor. The framework capabilities below depend on the firmware profile; template demo features are not the actuator startup experience.

## Features

### :butterfly: Beautiful UI powered by DaisyUI and TailwindCSS

Beautiful, responsive UI which works equally well on desktop and on mobile. Gently animated for a snappy and modern feeling without ever being obtrusive or in the way. Easy theming with DaisyUI and media-queries to respect the users wish for a light or dark theme.

### :simple-svelte: Low Memory Footprint and Easy Customization by Courtesy of SvelteKit

SvelteKit is ideally suited to be served from constrained devices like an ESP32. It's unique approach leads to very slim files. No bloatware like other popular JS frameworks. Not only the low memory footprint make it ideal but the developer experience is also outstanding letting you customize the front end with ease. Adapt and add functionality as you need it. The back end has you covered as well.

### :telephone: Rich Communication Interfaces

Comes with a rich set of communication interfaces to cover most standard needs of an IoT application. Like MQTT client, HTTP RESTful API, a WebSocket based Event Socket and a classic Websocket Server. All communication channels are stateful and fully synchronized. Changes propagate and are communicated to all other participants. The states can be persisted on the file system as well. For accurate time keeping time can by synchronized over NTP.

### :file_cabinet: WiFi Provisioning and Management

Naturally ESP32 SvelteKit comes with rich features to manage all your WiFi needs. From pulling up an access point for provisioning or as fall back, to fully manage your WiFi networks. Scan for available networks and connect to them. Advanced configuration options like static IP are on board as well.

### :people_with_bunny_ears_partying: Secured API and User Management

Manage different user of your app with two authorization levels. An administrator and a guest user. Authenticate their API calls with a JWT token. Manage the user's profile from the admin interface. Use at own risk, as it is neither secure without the ability to use TLS/SSL encryption on the ESP32 server, nor very convenient, as only an admin can change passwords.

### :airplane: OTA Upgrade Service

The framework can provide two different channels for Over-the-Air updates. Either by uploading a \*.bin file from the web interface. Or by pulling a firmware image from an update server. This is implemented with the github release page as an example. It is even possible to have different build environments at the same time and the Github OTA process pulls the correct binary.

### :construction_site: Automated Build Chain

The automated build chain takes out the pain and tears of getting all the bits and pieces play nice together. The repository contains a PlatformIO project at its heart. A SvelteKit project for the frontend code and a mkdocs project for the documentation go alongside. The PlatformIO build tools not only build the SvelteKit frontend with Vite, but also ensure that the build results are gzipped and find their way into the flash memory of the ESP32. You have two choices to serve the frontend either from the flash partition, or embedded into the firmware binary. The latter is much more friendly if your frontend code should be distributed OTA as well, leaving all configuration files intact.

### :fontawesome-solid-microchip: Compatible with all ESP32 Flavours

The code runs on all variants of the ESP32 chip family. From the plain old ESP32, the ESP32-S3 and ESP32-C3. Other ESP32 variants might work, but haven't been tested. Sorry, no support for the older ESP8266. Go with one of the ESP32's instead.

[Let's get started!](gettingstarted.md)

## License

ESP32 SvelteKit is distributed with two licenses for different sections of the code. The back end code inherits the GNU LESSER GENERAL PUBLIC LICENSE Version 3 and is therefore distributed with said license. The front end code is distributed under the MIT License. See the [LICENSE](https://github.com/theelims/ESP32-sveltekit/blob/main/LICENSE) for a full text of both licenses.
