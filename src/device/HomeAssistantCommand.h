#pragma once
#include <cstring>

namespace actuator
{
// Exact, idempotent commands only. Retained commands must never operate hardware.
inline int homeAssistantCommand(const char *suffix, const char *payload, bool retained)
{
    if (retained || !suffix || !payload)
    {
        return -1;
    }
    if (!std::strcmp(suffix, "identify/set") && !std::strcmp(payload, "PRESS"))
    {
        return 12;
    }
    if (std::strlen(suffix) != 11 || std::strncmp(suffix, "relay/", 6) || suffix[6] < '1' || suffix[6] > '6' ||
        std::strcmp(suffix + 7, "/set"))
    {
        return -1;
    }
    const int channel = suffix[6] - '1';
    if (!std::strcmp(payload, "ON"))
    {
        return channel * 2 + 1;
    }
    if (!std::strcmp(payload, "OFF"))
    {
        return channel * 2;
    }
    return -1;
}
} // namespace actuator
