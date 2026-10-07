/**
 *   ESP32 SvelteKit
 *
 *   A simple, secure and extensible framework for IoT projects for ESP32 platforms
 *   with responsive Sveltekit front-end built with TailwindCSS and DaisyUI.
 *   https://github.com/theelims/ESP32-sveltekit
 *
 *   Copyright (C) 2018 - 2023 rjwats
 *   Copyright (C) 2023 - 2025 theelims
 *   Copyright (C) 2026 - Present MTech
 *
 *   All Rights Reserved. This software may be modified and distributed under
 *   the terms of the LGPL v3 license. See the LICENSE file for details.
 **/

#include <ESP32SvelteKit.h>
#include <PsychicHttpServer.h>

#ifdef ACTUATOR_BOARD
#include "device/Actuator.h"
#else
#include <LightMqttSettingsService.h>
#include <LightStateService.h>
#endif

PsychicHttpServer server;
#ifdef ACTUATOR_BOARD
ESP32SvelteKit esp32sveltekit(&server, 160);
actuator::Actuator device(esp32sveltekit);
#else
ESP32SvelteKit esp32sveltekit(&server, 70);
LightMqttSettingsService lightMqttSettingsService(&server, &esp32sveltekit);
LightStateService lightStateService(&server, &esp32sveltekit, &lightMqttSettingsService);
#endif

void setup()
{
#ifdef ACTUATOR_BOARD
    actuator::Actuator::safePins();
#endif
    Serial.begin(115200);
#if defined(ACTUATOR_BOARD) && ARDUINO_USB_MODE && ARDUINO_USB_CDC_ON_BOOT
    // Diagnostics must never hold up HTTP/control tasks when a USB host stops reading.
    Serial.setTxTimeoutMs(0);
#endif
#ifdef ACTUATOR_BOARD
    const String setupPassword = SetupIdentity::password();
    if (setupPassword.isEmpty())
    {
        Serial.println("Setup identity unavailable; provisioning stopped");
        return;
    }
    Serial.printf("Device setup password (admin and factory AP): %s\n", setupPassword.c_str());
#endif
    esp32sveltekit.begin();
#ifdef ACTUATOR_BOARD
    device.begin();
#else
    lightStateService.begin();
    lightMqttSettingsService.begin();
#endif
}

void loop()
{
    vTaskDelete(nullptr);
}
