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

#include <EthernetStatus.h>

#if FT_ENABLED(FT_ETHERNET)

EthernetStatus::EthernetStatus(PsychicHttpServer *server, SecurityManager *securityManager)
    : _server(server), _securityManager(securityManager)
{
}

void EthernetStatus::begin()
{
    _server->on(ETHERNET_STATUS_SERVICE_PATH,
                HTTP_GET,
                _securityManager->wrapRequest(std::bind(&EthernetStatus::ethernetStatus, this, std::placeholders::_1),
                                              AuthenticationPredicates::IS_AUTHENTICATED));

    ESP_LOGV(SVK_TAG, "Registered GET endpoint: %s", ETHERNET_STATUS_SERVICE_PATH);

    // All interface events are dispatched by Network.
    Network.onEvent(onConnected, ARDUINO_EVENT_ETH_CONNECTED);
    Network.onEvent(onDisconnected, ARDUINO_EVENT_ETH_DISCONNECTED);
    Network.onEvent(onGotIP, ARDUINO_EVENT_ETH_GOT_IP);
}

void EthernetStatus::onConnected(arduino_event_id_t event, arduino_event_info_t info)
{
    ESP_LOGI(SVK_TAG, "Ethernet Connected.");
#ifdef SERIAL_INFO
    Serial.println("Ethernet Connected.");
#endif
}

void EthernetStatus::onDisconnected(arduino_event_id_t event, arduino_event_info_t info)
{
    ESP_LOGI(SVK_TAG, "Ethernet Disconnected.");
#ifdef SERIAL_INFO
    Serial.print("Ethernet Disconnected.");
#endif
}

void EthernetStatus::onGotIP(arduino_event_id_t event, arduino_event_info_t info)
{
    ESP_LOGI(SVK_TAG, "Ethernet Got IP. localIP=%s, hostName=%s", ETH.localIP().toString().c_str(), ETH.getHostname());
#ifdef SERIAL_INFO
    Serial.printf("Ethernet Got IP. localIP=%s, hostName=%s\r\n", ETH.localIP().toString().c_str(), ETH.getHostname());
#endif
}

esp_err_t EthernetStatus::ethernetStatus(PsychicRequest *request)
{
    PsychicJsonResponse response = PsychicJsonResponse(request, false);
    JsonObject root = response.getRoot();
    bool isConnected = ETH.connected();
    root["connected"] = isConnected;
    if (isConnected)
    {
        root["local_ip"] = ETH.localIP().toString();
        root["mac_address"] = ETH.macAddress();
        root["subnet_mask"] = ETH.subnetMask().toString();
        root["gateway_ip"] = ETH.gatewayIP().toString();
        IPAddress dnsIP1 = ETH.dnsIP(0);
        IPAddress dnsIP2 = ETH.dnsIP(1);
        if (IPUtils::isSet(dnsIP1))
        {
            root["dns_ip_1"] = dnsIP1.toString();
        }
        if (IPUtils::isSet(dnsIP2))
        {
            root["dns_ip_2"] = dnsIP2.toString();
        }
        root["link_speed"] = ETH.linkSpeed();
    }

    return response.send();
}

bool EthernetStatus::isConnected()
{
    return ETH.connected();
}

#endif // end FT_ENABLED(FT_ETHERNET)
