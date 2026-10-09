#pragma once
#include <Features.h>
#if FT_ENABLED(FT_XIAOZHI_MCP)
#include "XiaozhiMcpPulseCache.h"
#include <XiaozhiMcpService.h>
#include <memory>

namespace actuator
{
class Actuator;

class XiaozhiMcpAdapter
{
  public:
    XiaozhiMcpAdapter(Actuator &device, XiaozhiMcpService &service) : device(device), service(service)
    {
    }

    void begin();
    void loop();

  private:
    Actuator &device;
    XiaozhiMcpService &service;
    QueueHandle_t queue = nullptr;

    struct Request
    {
        xiaozhi::Operation operation;
        uint32_t generation, deadline;
        std::string args, result;
        bool ok = false;
        SemaphoreHandle_t done = xSemaphoreCreateBinary();

        ~Request()
        {
            if (done)
            {
                vSemaphoreDelete(done);
            }
        }
    };

    xiaozhi::PulseCache pulses;

    bool submit(xiaozhi::Operation op, JsonObjectConst args, JsonObject result, uint32_t generation);
};
} // namespace actuator
#endif
