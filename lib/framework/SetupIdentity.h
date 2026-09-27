#pragma once
#include <Arduino.h>
#include <esp_system.h>
#include <nvs.h>

// Manufacturing identity is deliberately outside resettable /config files.
namespace SetupIdentity
{
inline String password()
{
    static String value;
    if (value.length())
    {
        return value;
    }
    nvs_handle_t handle;
    if (nvs_open("actuator-id", NVS_READWRITE, &handle) != ESP_OK)
    {
        return String();
    }
    char saved[25] = {};
    size_t length = sizeof(saved);
    if (nvs_get_str(handle, "setup", saved, &length) == ESP_OK && length == sizeof(saved))
    {
        value = saved;
    }
    if (value.isEmpty())
    {
        const char *hex = "0123456789abcdef";
        for (int i = 0; i < 6; ++i)
        {
            uint32_t random = esp_random();
            for (int j = 0; j < 4; ++j)
            {
                value += hex[(random >> (j * 8)) & 15];
            }
        }
        if (nvs_set_str(handle, "setup", value.c_str()) != ESP_OK || nvs_commit(handle) != ESP_OK)
        {
            value = "";
        }
    }
    nvs_close(handle);
    return value;
}
} // namespace SetupIdentity
