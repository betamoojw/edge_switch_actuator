#pragma once
#include <cstdint>
#include <functional>
inline uint32_t testNow = 0;
inline std::function<void()> testDelay;

inline uint32_t millis()
{
    return testNow;
}

inline void delay(uint32_t ms)
{
    testNow += ms;
    if (testDelay)
    {
        testDelay();
    }
}
