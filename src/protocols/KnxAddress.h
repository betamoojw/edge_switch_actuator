#pragma once
#include <cstdint>

namespace actuator
{
// Match the commissioning form's area.line.device notation exactly.
inline bool parseIndividualAddress(const char *text, uint16_t &address)
{
    if (!text)
    {
        return false;
    }
    unsigned parts[3] = {};
    for (unsigned part = 0; part < 3; ++part)
    {
        unsigned digits = 0;
        while (*text >= '0' && *text <= '9')
        {
            if (++digits > (part == 2 ? 3u : 2u))
            {
                return false;
            }
            parts[part] = parts[part] * 10 + unsigned(*text++ - '0');
        }
        if (!digits || parts[part] > (part == 2 ? 255u : 15u))
        {
            return false;
        }
        if (part < 2 && *text++ != '.')
        {
            return false;
        }
    }
    if (*text || parts[2] == 0)
    {
        return false;
    }
    address = uint16_t((parts[0] << 12) | (parts[1] << 8) | parts[2]);
    return true;
}
} // namespace actuator
