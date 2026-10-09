#pragma once
#include <XiaozhiMcpProtocol.h>

namespace xiaozhi
{
// Owned by the actuator task, so retries survive transport reconnections.
class PulseCache
{
    struct Entry
    {
        std::string id, args, result;
        uint32_t at = 0;
        bool ok = false;
    } entries[16];

  public:
    template <class Execute>
    bool run(const std::string &id, const std::string &fingerprint, uint32_t now, JsonObject out, Execute execute)
    {
        for (auto &entry : entries)
        {
            if (entry.id == id && uint32_t(now - entry.at) < 60000)
            {
                if (entry.args != fingerprint)
                {
                    out["error"] = "operation_id_reused";
                    return false;
                }
                JsonDocument previous;
                deserializeJson(previous, entry.result);
                out.set(previous.as<JsonObjectConst>());
                return entry.ok;
            }
        }
        for (auto &entry : entries)
        {
            if (entry.id.empty() || uint32_t(now - entry.at) >= 60000)
            {
                bool ok = execute();
                std::string wire;
                serializeJson(out, wire);
                entry = {id, fingerprint, wire, now, ok};
                return ok;
            }
        }
        out["error"] = "pulse_retry_capacity";
        return false;
    }
};
} // namespace xiaozhi
