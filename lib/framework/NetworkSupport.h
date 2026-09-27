#pragma once
#include <Network.h>
#include <cstring>

// Shared IPv4 uplink view. Provisioning AP is never an outbound/service uplink.
// The core owns route selection and interface lifetimes.
namespace NetworkSupport
{
inline NetworkInterface *uplink()
{
    auto *interface = Network.getDefaultInterface();
    if (!interface || !interface->connected() || !interface->hasIP() ||
        (interface->ifkey() && strcmp(interface->ifkey(), "WIFI_AP_DEF") == 0))
    {
        return nullptr;
    }
    return interface;
}

inline bool online()
{
    return uplink() != nullptr;
}

inline IPAddress localIP()
{
    auto *interface = uplink();
    return interface ? interface->localIP() : IPAddress();
}

inline int interfaceIndex()
{
    auto *interface = uplink();
    return interface ? interface->impl_index() : -1;
}

inline String interfaceName()
{
    auto *interface = uplink();
    return interface ? interface->impl_name() : String();
}
} // namespace NetworkSupport
