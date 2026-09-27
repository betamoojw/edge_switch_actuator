#pragma once
#include <cstdint>
#include <string>
using String = std::string;

struct IPAddress
{
    uint32_t value = 0;

    IPAddress(uint32_t value = 0) : value(value)
    {
    }

    bool operator==(IPAddress other) const
    {
        return value == other.value;
    }
};

struct Netif
{
};

inline Netif accessPoint;

struct NetworkInterface
{
    bool link = false, address = false;
    Netif *handle = nullptr;
    int index = -1;
    IPAddress ip;
    String name;

    bool connected() const
    {
        return link;
    }

    bool hasIP() const
    {
        return address;
    }

    const char *ifkey() const
    {
        return handle == &accessPoint ? "WIFI_AP_DEF" : "TEST_IF";
    }

    IPAddress localIP() const
    {
        return ip;
    }

    int impl_index() const
    {
        return index;
    }

    String impl_name() const
    {
        return name;
    }
};

struct NetworkManager
{
    NetworkInterface *selected = nullptr;

    NetworkInterface *getDefaultInterface()
    {
        return selected;
    }
};

inline NetworkManager Network;
