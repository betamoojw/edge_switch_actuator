#include "DeviceConfig.h"
#include <IPAddress.h>

namespace actuator
{
void Config::write(JsonObject o) const
{
    o["schemaVersion"] = 1;
    o["revision"] = revision;
    auto a = o["relays"].to<JsonArray>();
    for (auto &r : relays)
    {
        auto j = a.add<JsonObject>();
        j["name"] = r.name;
        j["enabled"] = r.enabled;
        j["startup"] = r.startup;
        j["pulseMs"] = r.pulseMs;
        j["disconnectOff"] = r.disconnectOff;
        j["timeout"] = r.timeout;
    }
    o["rgb"] = rgb;
    o["buzzer"] = buzzer;
    o["button"] = button;
    o["rs485"] = rs485;
    o["brightness"] = brightness;
    auto b = o["clicks"].to<JsonArray>();
    for (auto &v : clicks)
    {
        auto j = b.add<JsonObject>();
        j["action"] = v.action;
        j["target"] = v.target;
    }
    o["mode"] = modeName(mode);
    o["unit"] = unit;
    o["baud"] = baud;
    o["serialFormat"] = serialFormat;
    o["tcpPort"] = tcpPort;
    o["busMask"] = busMask;
    o["busIndicators"] = busIndicators;
    o["busWatchdog"] = busWatchdog;
    o["tcpAllow"] = tcpAllow;
}

bool Config::parse(JsonObjectConst o, Config &c, String &error)
{
    auto bad = [&](const char *s)
    {
        error = s;
        return false;
    };
    auto number = [](JsonVariantConst v, int lo, int hi) { return v.is<int>() && v.as<int>() >= lo && v.as<int>() <= hi; };
    if (o["schemaVersion"] != 1 || !o["revision"].is<uint32_t>())
    {
        return bad("Invalid schema/revision");
    }
    if (!o["relays"].is<JsonArrayConst>() || o["relays"].size() != 6 || o["clicks"].size() != 3)
    {
        return bad("Expected six channels and three bindings");
    }
    Config n;
    n.revision = o["revision"];
    for (int i = 0; i < 6; ++i)
    {
        auto r = o["relays"][i];
        auto &v = n.relays[i];
        if (!r["name"].is<const char *>() || r["name"].as<String>().length() > 32 || !r["enabled"].is<bool>() || !r["startup"].is<bool>() ||
            !r["disconnectOff"].is<bool>() || !number(r["pulseMs"], 10, 60000) || !number(r["timeout"], 1, 3600))
        {
            return bad("Invalid relay settings");
        }
        v.name = r["name"].as<String>();
        v.enabled = r["enabled"];
        v.startup = r["startup"];
        v.disconnectOff = r["disconnectOff"];
        v.pulseMs = r["pulseMs"];
        v.timeout = r["timeout"];
    }
    for (auto key : {"rgb", "buzzer", "button", "rs485", "busIndicators", "busWatchdog"})
    {
        if (!o[key].is<bool>())
        {
            return bad("Expected boolean setting");
        }
    }
    n.rgb = o["rgb"];
    n.buzzer = o["buzzer"];
    n.button = o["button"];
    n.rs485 = o["rs485"];
    n.busIndicators = o["busIndicators"];
    n.busWatchdog = o["busWatchdog"];
    if (!number(o["brightness"], 0, 100) || !number(o["unit"], 1, 247) || !number(o["baud"], 0, 4) || !number(o["serialFormat"], 0, 3) ||
        !number(o["tcpPort"], 1, 65535) || !number(o["busMask"], 0, 63))
    {
        return bad("Invalid interface or indicator settings");
    }
    n.brightness = o["brightness"];
    n.unit = o["unit"];
    n.baud = o["baud"];
    n.serialFormat = o["serialFormat"];
    n.tcpPort = o["tcpPort"];
    n.busMask = o["busMask"];
    if (n.tcpPort == 80)
    {
        return bad("Port 80 is reserved for management");
    }
    String mode = o["mode"] | "";
    if (mode == "off")
    {
        n.mode = Off;
    }
    else if (mode == "modbus_rtu")
    {
        n.mode = Rtu;
    }
    else if (mode == "modbus_tcp")
    {
        n.mode = Tcp;
    }
    else if (mode == "knx_ip")
    {
        n.mode = Knx;
    }
    else
    {
        return bad("Invalid protocol mode");
    }
    if (n.mode == Rtu && !n.rs485)
    {
        return bad("Enable RS485 before selecting RTU");
    }
    n.tcpAllow = o["tcpAllow"] | "";
    if (n.tcpAllow.length())
    {
        IPAddress ip;
        if (!ip.fromString(n.tcpAllow))
        {
            return bad("TCP source must be an IPv4 address or empty");
        }
    }
    for (int i = 0; i < 3; ++i)
    {
        auto b = o["clicks"][i];
        if (!number(b["action"], 0, 8) || !number(b["target"], 0, 6))
        {
            return bad("Invalid button binding");
        }
        n.clicks[i] = {b["action"].as<uint8_t>(), b["target"].as<uint8_t>()};
        auto v = n.clicks[i];
        if (v.action >= 1 && v.action <= 4)
        {
            if (!v.target || !n.relays[v.target - 1].enabled)
            {
                return bad("Button action requires enabled channel");
            }
        }
        else if (v.target)
        {
            return bad("Device action target must be zero");
        }
    }
    c = n;
    return true;
}
} // namespace actuator
