#pragma once
#include <ESP32SvelteKit.h>
#include <array>

namespace actuator
{
struct Actuator
{
    struct Relay
    {
        String name = "Channel";
        bool enabled = true;
    };

    struct Config
    {
        std::array<Relay, 6> relays;
        bool button = true;
    } config;

    FS fs;
    bool outputs[6] = {}, blocked[6] = {};
    int identified = 0, writes = 0;
    String source;

    void identify()
    {
        ++identified;
    }

    bool relay(int c, bool on, const char *origin)
    {
        if (!config.relays[c].enabled || blocked[c])
        {
            return false;
        }
        outputs[c] = on;
        source = origin;
        ++writes;
        return true;
    }

    void snapshot(JsonObject o)
    {
        o["uptime"] = 42;
        o["audit"] = "private";
        auto relays = o["relays"].to<JsonArray>();
        for (int c = 0; c < 6; ++c)
        {
            auto r = relays.add<JsonObject>();
            r["on"] = outputs[c];
            r["enabled"] = config.relays[c].enabled;
            r["blocked"] = blocked[c];
        }
    }
};
} // namespace actuator
