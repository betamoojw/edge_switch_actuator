#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <array>

namespace actuator
{
enum Mode : uint8_t
{
    Off,
    Rtu,
    Tcp,
    Knx
};

inline const char *modeName(Mode m)
{
    return m == Rtu ? "modbus_rtu" : m == Tcp ? "modbus_tcp" : m == Knx ? "knx_ip" : "off";
}

struct RelayConfig
{
    bool enabled = true, startup = false, disconnectOff = false;
    uint16_t pulseMs = 1000, timeout = 30;
    String name;
};

struct Binding
{
    uint8_t action = 0, target = 0;
};

struct Config
{
    uint32_t revision = 1;
    std::array<RelayConfig, 6> relays;
    bool rgb = true, buzzer = true, button = true, rs485 = true;
    uint8_t brightness = 10;
    Binding clicks[3] = {{0, 0}, {6, 0}, {8, 0}};
    Mode mode = Off;
    uint8_t unit = 1, baud = 1, serialFormat = 0, busMask = 63;
    uint16_t tcpPort = 502;
    bool busIndicators = false, busWatchdog = false;
    String tcpAllow = ""; // Exact source IP, empty permits station-network peers.

    Config()
    {
        for (int i = 0; i < 6; ++i)
        {
            relays[i].name = String("Channel ") + (i + 1);
        }
    }

    void write(JsonObject o) const;
    static bool parse(JsonObjectConst o, Config &c, String &error);
};
} // namespace actuator
