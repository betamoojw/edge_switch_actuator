#pragma once
#include <XiaozhiMcpProtocol.h>

namespace xiaozhi
{
// Runs only on the actuator task, under its lock and the connection authorization lock.
// Template keeps the actual production command logic testable without ESP32 GPIO.
template <class Device>
bool command(Device &device, Operation op, JsonObjectConst args, uint8_t mask, JsonObject result)
{
    auto reject = [&](const char *error)
    {
        result["error"] = error;
        return false;
    };
    if (op == Operation::Status)
    {
        result["revision"] = device.config.revision;
        result["protocol"] = unsigned(device.config.mode);
        result["fault"] = device.fault;
        auto relays = result["relays"].to<JsonArray>();
        for (int c = 0; c < 6; ++c)
        {
            if (mask & (1 << c))
            {
                auto relay = relays.add<JsonObject>();
                relay["channel"] = c + 1;
                relay["name"] = device.config.relays[c].name;
                relay["on"] = device.outputs[c];
                relay["enabled"] = device.config.relays[c].enabled;
                relay["blocked"] = device.blocks[c];
            }
        }
        return true;
    }
    if (op == Operation::Identify)
    {
        if (!device.config.rgb)
        {
            return reject("indicator_disabled");
        }
        device.identify();
        result["duration_ms"] = 5000;
        return true;
    }
    if (op == Operation::Relay || op == Operation::Pulse)
    {
        if (!args["channel"].is<unsigned>() || args["channel"].as<unsigned>() < 1 || args["channel"].as<unsigned>() > 6)
        {
            return reject("invalid_channel");
        }
        int c = args["channel"].as<int>() - 1;
        if (!(mask & (1 << c)))
        {
            return reject("channel_denied");
        }
        if (op == Operation::Relay && !args["on"].is<bool>())
        {
            return reject("invalid_value");
        }
        bool value = op == Operation::Pulse || args["on"].as<bool>();
        if (!device.relay(c, value, "xiaozhi_mcp", op == Operation::Pulse))
        {
            return reject("channel_unavailable");
        }
        result["channel"] = c + 1;
        result["on"] = device.outputs[c];
        if (op == Operation::Pulse)
        {
            result["duration_ms"] = device.config.relays[c].pulseMs;
        }
        return true;
    }
    if (op == Operation::AllOff)
    {
        if (!mask)
        {
            return reject("channel_denied");
        }
        for (int c = 0; c < 6; ++c)
        {
            if ((mask & (1 << c)) && device.config.relays[c].enabled && device.blocks[c])
            {
                return reject("channel_unavailable");
            }
        }
        for (int c = 0; c < 6; ++c)
        {
            if ((mask & (1 << c)) && device.config.relays[c].enabled)
            {
                device.relay(c, false, "xiaozhi_mcp");
            }
        }
        result["channel_mask"] = mask;
        return true;
    }
    return reject("unknown_operation");
}
} // namespace xiaozhi
