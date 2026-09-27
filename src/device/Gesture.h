#pragma once
#include <stdint.h>

// Wrap-safe, polling-only recognizer. No GPIO, allocation or RTOS dependency.
class Gesture
{
    public:
        enum Event
        {
            None,
            Single,
            Double,
            Triple,
            Reset
        };

        Event update(bool down, uint32_t now)
        {
            if (down != raw)
            {
                raw = down;
                edge = now;
            }
            if (uint32_t(now - edge) >= 30 && stable != raw)
            {
                stable = raw;
                if (!ready)
                {
                    if (!stable)
                    {
                        ready = true;
                    }
                    return None;
                }
                if (stable)
                {
                    pressedAt = now;
                    fired = false;
                }
                else if (!fired)
                {
                    if (uint32_t(now - pressedAt) <= 700)
                    {
                        if (clicks < 4)
                        {
                            ++clicks;
                        }
                        releasedAt = now;
                    }
                    else
                    {
                        clicks = 0;
                    }
                }
            }
            if (!ready && !down && uint32_t(now - edge) >= 30)
            {
                ready = true;
            }
            if (ready && stable && !fired && uint32_t(now - pressedAt) >= 10000)
            {
                fired = true;
                clicks = 0;
                return Reset;
            }
            if (stable && uint32_t(now - pressedAt) > 700)
            {
                clicks = 0;
            }
            if (!stable && clicks && uint32_t(now - releasedAt) >= 350)
            {
                auto count = clicks;
                clicks = 0;
                return count <= 3 ? Event(count) : None;
            }
            return None;
        }

        bool pressed() const
        {
            return stable;
        }

        bool armed(uint32_t now) const
        {
            return ready && stable && uint32_t(now - pressedAt) >= 5000;
        }

    private:
        bool raw = true, stable = true, ready = false, fired = false;
        uint8_t clicks = 0;
        uint32_t edge = 0, pressedAt = 0, releasedAt = 0;
};
