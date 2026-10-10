#include "XiaozhiMcpService.h"
#if FT_ENABLED(FT_XIAOZHI_MCP)
#include "XiaozhiMcpTransport.h"
#include <ConnectionLifecycle.h>
#include <NetworkSupport.h>
#include <SettingValue.h>
#include <esp_random.h>
#include <time.h>

namespace
{
class Lock
{
    SemaphoreHandle_t mutex;

  public:
    explicit Lock(SemaphoreHandle_t mutex) : mutex(mutex)
    {
        xSemaphoreTakeRecursive(mutex, portMAX_DELAY);
    }

    ~Lock()
    {
        xSemaphoreGiveRecursive(mutex);
    }
};

esp_err_t failure(PsychicRequest *r, int status, const char *error, const char *field = "")
{
    JsonDocument doc;
    doc["error"] = error;
    doc["field"] = field;
    String body;
    serializeJson(doc, body);
    return r->reply(status, "application/json", body.c_str());
}
} // namespace

XiaozhiMcpService::XiaozhiMcpService(PsychicHttpServer *server, FS *fs, SecurityManager *security)
    : server(server), security(security), fs(fs), store(*fs), mutex(xSemaphoreCreateRecursiveMutex())
{
}

void XiaozhiMcpService::begin()
{
    deviceId = SettingValue::format("#{unique_id}");
    String data;
    if (store.read("/config/xiaozhiMcp", data))
    {
        JsonDocument doc;
        storageValid = !deserializeJson(doc, data) && xiaozhi::Settings::restore(doc.as<JsonObjectConst>(), settings);
    }
    else if (fs->exists("/config/xiaozhiMcp.0") || fs->exists("/config/xiaozhiMcp.1"))
    {
        storageValid = false;
    }
    if (!storageValid)
    {
        setState("error", "configuration_invalid");
    }
    auto get = [this](const char *path, bool admin, std::function<void(JsonObject)> read)
    {
        server->on(path, HTTP_GET,
                   [this, admin, read](PsychicRequest *request)
                   {
                       auto auth = security->authenticateRequest(request);
                       if (!auth.authenticated)
                       {
                           return request->reply(401);
                       }
                       if (admin && !auth.user->admin)
                       {
                           return request->reply(403);
                       }
                       PsychicJsonResponse response(request, false);
                       {
                           Lock lock(mutex);
                           read(response.getRoot());
                       }
                       return response.send();
                   });
    };
    get("/rest/xiaozhiMcpSettings", true, [this](JsonObject out) { settings.write(out); });
    // Reveal only on an explicit administrator request; ordinary reads stay redacted.
    server->on("/rest/xiaozhiMcpEndpoint", HTTP_POST,
               [this](PsychicRequest *request, JsonVariant &body)
               {
                   auto auth = security->authenticateRequest(request);
                   if (!auth.authenticated) return request->reply(401);
                   if (!auth.user->admin) return request->reply(403);
                   if (!body.is<JsonObject>() || body.size() != 1 || !body["revision"].is<uint32_t>())
                       return failure(request, 422, "invalid_settings", "revision");
                   PsychicJsonResponse response(request, false);
                   response.addHeader("Cache-Control", "no-store");
                   int code = 200;
                   {
                       Lock lock(mutex);
                       if (!storageValid) code = 503;
                       else if (body["revision"].as<uint32_t>() != settings.revision) code = 409;
                       else
                       {
                           response.getRoot()["endpoint"] = settings.endpoint;
                           response.getRoot()["revision"] = settings.revision;
                       }
                   }
                   if (code != 200) return failure(request, code, "endpoint_unavailable");
                   return response.send();
               });
    get("/rest/xiaozhiMcpStatus", false, [this](JsonObject out) { status(out); });
    get("/rest/xiaozhiMcpTools", true,
        [this](JsonObject out)
        {
            xiaozhi::tools(out["tools"].to<JsonArray>(), deviceId.c_str(), settings);
            out["channel_mask"] = settings.mask;
        });
    server->on("/rest/xiaozhiMcpSettings", HTTP_POST, [this](PsychicRequest *r, JsonVariant &json) { return save(r, json); });
    server->on("/rest/xiaozhiMcpReconnect", HTTP_POST,
               [this](PsychicRequest *request)
               {
                   auto auth = security->authenticateRequest(request);
                   if (!auth.authenticated)
                   {
                       return request->reply(401);
                   }
                   if (!auth.user->admin)
                   {
                       return request->reply(403);
                   }
                   int code = 202;
                   const char *error = "";
                   {
                       Lock lock(mutex);
                       if (!settings.enabled || settings.endpoint.empty())
                       {
                           code = 409;
                           error = "not_enabled";
                       }
                       else if (!workerStarted || stopped || ConnectionLifecycle::paused() || !storageValid)
                       {
                           code = 503;
                           error = "service_unavailable";
                       }
                       else
                       {
                           ++generation;
                           ready = connected = false;
                           state = "connecting";
                       }
                   }
                   if (code != 202)
                   {
                       return failure(request, code, error);
                   }
                   return request->reply(202, "application/json", "{\"accepted\":true}");
               });
    // A pending factory reset must never briefly connect using the old credential.
    if (fs->exists("/reset.pending"))
    {
        stopped = true;
        setState("paused");
    }
    workerStarted = xTaskCreatePinnedToCore([](void *p) { static_cast<XiaozhiMcpService *>(p)->loop(); }, "XiaozhiMCP", 8192, this, 1,
                                            nullptr, ESP32SVELTEKIT_RUNNING_CORE) == pdPASS;
    if (!workerStarted)
    {
        setState("error", "worker_unavailable");
    }
}

void XiaozhiMcpService::attach(Handler value)
{
    Lock lock(mutex);
    handler = std::move(value);
    provider = true;
    ++generation;
}

void XiaozhiMcpService::stop()
{
    Lock lock(mutex);
    stopped = true;
    ++generation;
    ready = connected = false;
    state = "paused";
}

bool XiaozhiMcpService::authorized(uint32_t expected, const std::function<bool(const xiaozhi::Settings &)> &operation)
{
    Lock lock(mutex);
    return generation == expected && settings.enabled && ready && !stopped && !ConnectionLifecycle::paused() && NetworkSupport::online() &&
           time(nullptr) >= 1704067200 && operation(settings);
}

void XiaozhiMcpService::setState(const char *value, const char *error)
{
    Lock lock(mutex);
    state = value;
    errorCode = error;
}

void XiaozhiMcpService::status(JsonObject out)
{
    out["state"] = state;
    out["error_code"] = errorCode;
    out["enabled"] = settings.enabled;
    out["connected"] = connected;
    out["ready"] = ready;
    out["alias"] = settings.alias;
    out["device_id"] = deviceId;
    out["network_interface"] = networkInterface;
    out["tool_count"] = provider ? (settings.mask ? 6 : 3) : 0;
    out["retry_in_ms"] = retryAt && int32_t(retryAt - millis()) > 0 ? retryAt - millis() : 0;
    out["connected_at_ms"] = connectedAt;
}

esp_err_t XiaozhiMcpService::save(PsychicRequest *request, JsonVariant &body)
{
    auto auth = security->authenticateRequest(request);
    if (!auth.authenticated)
    {
        return request->reply(401);
    }
    if (!auth.user->admin)
    {
        return request->reply(403);
    }
    if (!body.is<JsonObject>() || measureJson(body) > 4096)
    {
        return failure(request, 400, "invalid_body");
    }
    PsychicJsonResponse response(request, false);
    const char *error = "";
    std::string field;
    const int result = [&]() -> int
    {
        Lock lock(mutex);
        if (stopped || ConnectionLifecycle::paused())
        {
            error = "service_unavailable";
            return 503;
        }
        // Preserve unsupported/corrupt records for diagnosis; reset explicitly to erase them.
        if (!storageValid)
        {
            error = "configuration_invalid";
            return 503;
        }
        xiaozhi::Settings next;
        int code = xiaozhi::Settings::update(body.as<JsonObjectConst>(), settings, next, field);
        if (code != 200)
        {
            error = code == 409 ? "revision_conflict" : "invalid_settings";
            return code;
        }
        if (!(next == settings))
        {
            JsonDocument doc;
            next.write(doc.to<JsonObject>(), true);
            String data;
            serializeJson(doc, data);
            if (!store.write("/config/xiaozhiMcp", data))
            {
                error = "storage_failed";
                return 503;
            }
            settings = std::move(next);
            ++generation;
            ready = connected = false;
            state = settings.enabled ? "connecting" : "disabled";
            errorCode = "";
        }
        settings.write(response.getRoot());
        return 200;
    }();
    // HTTP sends can block on the peer; release the authorization lock first.
    return result == 200 ? response.send() : failure(request, result, error, field.c_str());
}

void XiaozhiMcpService::loop()
{
    xiaozhi::Transport transport;
    xiaozhi::Protocol protocol(deviceId.c_str(), APP_VERSION);
    uint32_t active = 0, attemptAt = 0, readyAt = 0, backoff = 1000;
    bool started = false, paused = false, failed = false, authFailure = false;
    int lastInterface = -1;
    IPAddress lastAddress;
    xiaozhi::Settings config;
    auto close = [&]()
    {
        // Invalidate before closing; no callback can authorize stale work.
        {
            Lock lock(mutex);
            ++generation;
            ready = connected = false;
            connectedAt = 0;
        }
        transport.close();
        protocol.reset();
        started = false;
        readyAt = 0;
        ConnectionLifecycle::mcpIdle = true;
    };
    for (;;)
    {
        bool enabled, available;
        {
            Lock lock(mutex);
            enabled = settings.enabled;
            available = provider && storageValid && !stopped && !ConnectionLifecycle::paused();
        }
        const int currentInterface = NetworkSupport::interfaceIndex();
        const IPAddress address = NetworkSupport::localIP();
        if (currentInterface != lastInterface || address != lastAddress)
        {
            close();
            active = 0;
            lastInterface = currentInterface;
            lastAddress = address;
        }
        if (!available || !enabled)
        {
            if (started || !paused)
            {
                close();
            }
            paused = true;
            {
                Lock lock(mutex);
                retryAt = 0;
                if (!storageValid)
                {
                    state = "error";
                    errorCode = "configuration_invalid";
                }
                else
                {
                    state = !enabled ? "disabled" : "paused";
                }
            }
        }
        else
        {
            paused = false;
            if (active != generation.load())
            {
                close();
                Lock lock(mutex);
                config = settings;
                active = generation;
                retryAt = 0;
                backoff = 1000;
            }
            if (!NetworkSupport::online())
            {
                if (started)
                {
                    close();
                }
                setState("waiting_network");
            }
            else if (time(nullptr) < 1704067200)
            {
                if (started)
                {
                    close();
                }
                setState("waiting_time");
            }
            else if (config.endpoint.empty())
            {
                setState("unconfigured");
            }
            else
            {
                uint32_t next;
                {
                    Lock lock(mutex);
                    next = retryAt;
                    networkInterface = NetworkSupport::interfaceName();
                }
                const uint32_t now = millis();
                if (!started && (!next || int32_t(now - next) >= 0))
                {
                    failed = authFailure = false;
                    protocol.reset();
                    attemptAt = now;
                    started = true;
                    setState("connecting");
                    ConnectionLifecycle::mcpIdle = false;
                    // An update may have started after this iteration's availability check.
                    bool canStart;
                    {
                        Lock lock(mutex);
                        canStart = settings.enabled && active == generation && !stopped && !ConnectionLifecycle::paused();
                    }
                    if (!canStart)
                    {
                        close();
                        continue;
                    }
                    transport.open(
                        config.endpoint,
                        [&](xiaozhi::Transport::Event event, const char *payload, size_t length)
                        {
                            if (event == xiaozhi::Transport::Event::Connected)
                            {
                                Lock lock(mutex);
                                if (failed || active != generation || ConnectionLifecycle::paused())
                                {
                                    return;
                                }
                                connected = true;
                                connectedAt = millis();
                                state = "initializing";
                            }
                            else if (event == xiaozhi::Transport::Event::Failed || event == xiaozhi::Transport::Event::Unauthorized)
                            {
                                failed = true;
                                if (event == xiaozhi::Transport::Event::Unauthorized)
                                {
                                    authFailure = true;
                                }
                                Lock lock(mutex);
                                ready = connected = false;
                            }
                            else if (event == xiaozhi::Transport::Event::Text)
                            {
                                if (failed || active != generation || ConnectionLifecycle::paused())
                                {
                                    return;
                                }
                                auto response = protocol.receive(payload, length, config, millis(),
                                                                 [&](xiaozhi::Operation op, JsonObjectConst args, JsonObject result)
                                                                 { return handler(op, args, result, active); });
                                {
                                    Lock lock(mutex);
                                    ready = protocol.ready() && active == generation;
                                    if (ready)
                                    {
                                        state = "ready";
                                        errorCode = "";
                                    }
                                }
                                if (protocol.ready() && !readyAt)
                                {
                                    readyAt = millis();
                                }
                                if (!response.empty() && !transport.send(response))
                                {
                                    failed = true;
                                }
                            }
                        });
                }
                if (started)
                {
                    transport.loop();
                }
                if (started && (failed || (!protocol.ready() && uint32_t(millis() - attemptAt) > 20000)))
                {
                    const uint32_t previous = active;
                    close();
                    active = previous + 1; // A concurrent settings update must still be noticed.
                    Lock lock(mutex);
                    if (authFailure)
                    {
                        backoff = 60000;
                    }
                    retryAt = millis() + backoff + esp_random() % 251;
                    backoff = std::min(uint32_t(60000), backoff * 2);
                    state = "backoff";
                    errorCode = authFailure ? "authentication_failed" : "connection_failed";
                }
                if (readyAt && uint32_t(millis() - readyAt) >= 30000)
                {
                    backoff = 1000;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
#endif
