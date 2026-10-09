#pragma once
#include <Arduino.h>
#include <atomic>

// Internal lifecycle signal, independent of browser subscriptions.
namespace ConnectionLifecycle
{
inline std::atomic<bool> updating {false}, stopping {false};
inline std::atomic<bool> mcpIdle {true};

inline bool paused()
{
    return updating.load() || stopping.load();
}

inline bool beginUpdate()
{
    updating = true;
    const uint32_t start = millis();
    while (!mcpIdle && uint32_t(millis() - start) < 3000)
    {
        delay(10);
    }
    if (mcpIdle)
    {
        return true;
    }
    updating = false;
    return false;
}
} // namespace ConnectionLifecycle
