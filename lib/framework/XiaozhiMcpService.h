#pragma once
#include <Features.h>
#if FT_ENABLED(FT_XIAOZHI_MCP)
#include "XiaozhiMcpProtocol.h"
#include <DurableStore.h>
#include <SecurityManager.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// The worker owns the transport. Settings and authorization are serialized by mutex.
class XiaozhiMcpService
{
  public:
    using Handler = std::function<bool(xiaozhi::Operation, JsonObjectConst, JsonObject, uint32_t)>;
    XiaozhiMcpService(PsychicHttpServer *server, FS *fs, SecurityManager *security);
    void begin();
    void attach(Handler handler);
    void stop();

    const String &identity() const
    {
        return deviceId;
    } // Immutable after begin(), before provider attachment.

    // Runs a short actuator operation while settings cannot change. Never call network I/O here.
    bool authorized(uint32_t generation, const std::function<bool(const xiaozhi::Settings &)> &operation);

  private:
    PsychicHttpServer *server;
    SecurityManager *security;
    FS *fs;
    DurableStore store;
    SemaphoreHandle_t mutex;
    xiaozhi::Settings settings;
    Handler handler;
    std::atomic<uint32_t> generation {1};
    bool provider = false, workerStarted = false, stopped = false, storageValid = true;
    const char *state = "disabled", *errorCode = "";
    bool connected = false, ready = false;
    uint32_t retryAt = 0, connectedAt = 0;
    String deviceId, networkInterface;
    void loop();
    void status(JsonObject out);
    void setState(const char *value, const char *error = "");
    esp_err_t save(PsychicRequest *request, JsonVariant &body);
};
#endif
