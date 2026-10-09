#include "XiaozhiMcpAdapter.h"
#if FT_ENABLED(FT_XIAOZHI_MCP)
#include "Actuator.h"
#include "XiaozhiMcpCommands.h"

namespace actuator
{
void XiaozhiMcpAdapter::begin()
{
    queue = xQueueCreate(4, sizeof(std::shared_ptr<Request> *));
    if (!queue)
    {
        return;
    }
    service.attach([this](xiaozhi::Operation op, JsonObjectConst args, JsonObject out, uint32_t generation)
                   { return submit(op, args, out, generation); });
}

bool XiaozhiMcpAdapter::submit(xiaozhi::Operation op, JsonObjectConst args, JsonObject result, uint32_t generation)
{
    auto request = std::make_shared<Request>();
    request->operation = op;
    request->generation = generation;
    request->deadline = millis() + 2000;
    serializeJson(args, request->args);
    auto owned = new std::shared_ptr<Request>(request);
    if (!request->done || xQueueSend(queue, &owned, 0) != pdTRUE)
    {
        delete owned;
        result["error"] = "queue_busy";
        return false;
    }
    if (xSemaphoreTake(request->done, pdMS_TO_TICKS(2100)) != pdTRUE)
    {
        result["error"] = "command_timeout";
        return false;
    }
    JsonDocument doc;
    if (deserializeJson(doc, request->result))
    {
        result["error"] = "command_failed";
        return false;
    }
    result.set(doc.as<JsonObjectConst>());
    return request->ok;
}

void XiaozhiMcpAdapter::loop()
{
    std::shared_ptr<Request> *owned = nullptr;
    if (!queue || xQueueReceive(queue, &owned, 0) != pdTRUE)
    {
        return;
    }
    auto request = *owned;
    delete owned;
    JsonDocument args, result;
    deserializeJson(args, request->args);
    auto out = result.to<JsonObject>();
    request->ok = service.authorized(
        request->generation,
        [&](const xiaozhi::Settings &settings)
        {
            if (int32_t(millis() - request->deadline) >= 0)
            {
                out["error"] = "command_expired";
                return false;
            }
            if (request->operation == xiaozhi::Operation::Alias)
            {
                out["alias"] = settings.alias;
                out["device_id"] = service.identity();
                return true;
            }
            const std::string id = args["request_id"] | std::string();
            if (request->operation == xiaozhi::Operation::Pulse)
            {
                if (!xiaozhi::validText(id, 64))
                {
                    out["error"] = "invalid_operation_id";
                    return false;
                }
                // Include policy revision to avoid authorizing a retry under a different exposure policy.
                const std::string fingerprint = std::to_string(settings.revision) + ":" + request->args;
                return pulses.run(id,
                                  fingerprint,
                                  millis(),
                                  out,
                                  [&]()
                                  { return xiaozhi::command(device, request->operation, args.as<JsonObjectConst>(), settings.mask, out); });
            }
            return xiaozhi::command(device, request->operation, args.as<JsonObjectConst>(), settings.mask, out);
        });
    if (!request->ok && !out["error"].is<const char *>())
    {
        out["error"] = "connection_changed";
    }
    device.audit(String("xiaozhi_mcp operation=") + unsigned(request->operation) + " channel=" + (args["channel"] | 0) +
                 (request->ok ? " applied" : " rejected"));
    serializeJson(result, request->result);
    xSemaphoreGive(request->done);
}
} // namespace actuator
#endif
