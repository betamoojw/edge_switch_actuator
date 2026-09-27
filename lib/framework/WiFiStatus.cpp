/**
 *   ESP32 SvelteKit
 *
 *   A simple, secure and extensible framework for IoT projects for ESP32 platforms
 *   with responsive Sveltekit front-end built with TailwindCSS and DaisyUI.
 *   https://github.com/theelims/ESP32-sveltekit
 *
 *   Copyright (C) 2018 - 2023 rjwats
 *   Copyright (C) 2023 - 2025 theelims
 *
 *   All Rights Reserved. This software may be modified and distributed under
 *   the terms of the LGPL v3 license. See the LICENSE file for details.
 **/

#include <WiFiStatus.h>

WiFiStatus::WiFiStatus(PsychicHttpServer *server, SecurityManager *securityManager) : _server(server), _securityManager(securityManager)
{
}

void WiFiStatus::begin()
{
    _server->on(WIFI_STATUS_SERVICE_PATH,
                HTTP_GET,
                _securityManager->wrapRequest(std::bind(&WiFiStatus::wifiStatus, this, std::placeholders::_1),
                                              AuthenticationPredicates::IS_AUTHENTICATED));

    ESP_LOGV(SVK_TAG, "Registered GET endpoint: %s", WIFI_STATUS_SERVICE_PATH);

    Network.onEvent(onStationModeConnected, ARDUINO_EVENT_WIFI_STA_CONNECTED);
    Network.onEvent(onStationModeDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    Network.onEvent(onStationModeGotIP, ARDUINO_EVENT_WIFI_STA_GOT_IP);
}

void WiFiStatus::onStationModeConnected(arduino_event_id_t event, arduino_event_info_t info)
{
    ESP_LOGI(SVK_TAG, "WiFi Connected.");

#ifdef SERIAL_INFO
    Serial.println("WiFi Connected.");
#endif
}

void WiFiStatus::onStationModeDisconnected(arduino_event_id_t event, arduino_event_info_t info)
{
    ESP_LOGI(SVK_TAG, "WiFi Disconnected. Reason code=%d", info.wifi_sta_disconnected.reason);

#ifdef SERIAL_INFO
    Serial.print("WiFi Disconnected. Reason code=");
    Serial.println(info.wifi_sta_disconnected.reason);
#endif
}

void WiFiStatus::onStationModeGotIP(arduino_event_id_t event, arduino_event_info_t info)
{
    ESP_LOGI(SVK_TAG, "WiFi Got IP. localIP=%s, hostName=%s", WiFi.STA.localIP().toString().c_str(), WiFi.STA.getHostname());
#ifdef SERIAL_INFO
    Serial.printf("WiFi Got IP. localIP=%s, hostName=%s\r\n", WiFi.STA.localIP().toString().c_str(), WiFi.STA.getHostname());
#endif
}

esp_err_t WiFiStatus::wifiStatus(PsychicRequest *request)
{
    PsychicJsonResponse response = PsychicJsonResponse(request, false);
    JsonObject root = response.getRoot();
    wl_status_t status = WiFi.status();
    root["status"] = (uint8_t) status;
    if (status == WL_CONNECTED)
    {
        root["local_ip"] = WiFi.STA.localIP().toString();
        root["mac_address"] = WiFi.STA.macAddress();
        root["rssi"] = WiFi.RSSI();
        root["ssid"] = WiFi.SSID();
        root["bssid"] = WiFi.BSSIDstr();
        root["channel"] = WiFi.channel();
        root["subnet_mask"] = WiFi.STA.subnetMask().toString();
        root["gateway_ip"] = WiFi.STA.gatewayIP().toString();
        IPAddress dnsIP1 = WiFi.STA.dnsIP(0);
        IPAddress dnsIP2 = WiFi.STA.dnsIP(1);
        if (IPUtils::isSet(dnsIP1))
        {
            root["dns_ip_1"] = dnsIP1.toString();
        }
        if (IPUtils::isSet(dnsIP2))
        {
            root["dns_ip_2"] = dnsIP2.toString();
        }
    }

    return response.send();
}

bool WiFiStatus::isConnected()
{
    return WiFi.status() == WL_CONNECTED;
}
