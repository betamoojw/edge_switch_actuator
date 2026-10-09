#include <ConnectionLifecycle.h>
#include <cassert>
#include <iostream>

int main()
{
    using namespace ConnectionLifecycle;
    assert(!paused() && beginUpdate() && paused());
    updating = false;
    mcpIdle = false;
    testDelay = []()
    {
        if (testNow >= 20)
        {
            mcpIdle = true;
        }
    };
    assert(beginUpdate() && testNow == 20 && paused());
    updating = false;
    mcpIdle = false;
    testDelay = nullptr;
    testNow = UINT32_MAX - 100;
    const auto before = testNow;
    assert(!beginUpdate() && !paused() && uint32_t(testNow - before) == 3000);
    stopping = true;
    assert(paused());
    std::cout << "MCP OTA pause, shutdown acknowledgement and timeout tests passed\n";
}
