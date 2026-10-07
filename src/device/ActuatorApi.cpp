#include "Actuator.h"
#include "protocols/KnxAdapter.h"
#include "protocols/Modbus.h"
#include <NetworkSupport.h>

namespace actuator
{
void Actuator::endpoint(const char *path)
{
    auto server = framework.getServer();
    server->on(path, HTTP_GET,
               [this, path](PsychicRequest *req)
               {
                   auto auth = framework.getSecurityManager()->authenticateRequest(req);
                   if (!auth.authenticated)
                   {
                       return req->reply(401);
                   }
                   PsychicJsonResponse response(req, false);
                   auto o = response.getRoot();
                   xSemaphoreTakeRecursive(mutex, portMAX_DELAY);
                   if (String(path) == "/rest/device/config")
                   {
                       config.write(o);
                   }
                   else if (String(path) == "/rest/knx/config")
                   {
                       knx->snapshot(o);
                   }
                   else
                   {
                       snapshot(o);
                   }
                   auto c = o["capabilities"].to<JsonObject>();
                   String role = auth.user->role;
                   bool installer = auth.user->admin || role == "installer";
                   c["configure"] = installer;
                   c["command"] = installer || role == "operator";
                   c["admin"] = auth.user->admin;
                   c["channels"] = auth.user->admin ? 63 : auth.user->channels;
                   xSemaphoreGiveRecursive(mutex);
                   return response.send();
               });
    if (String(path) == "/rest/device/status")
    {
        return;
    }
    server->on(path, HTTP_POST,
               [this, path](PsychicRequest *req, JsonVariant &json)
               {
                   auto auth = framework.getSecurityManager()->authenticateRequest(req);
                   if (!auth.authenticated)
                   {
                       return req->reply(401);
                   }
                   if (!json.is<JsonObject>() || measureJson(json) > 8192)
                   {
                       return req->reply(400);
                   }
                   bool installer = auth.user->admin || auth.user->role == "installer";
                   if (String(path) != "/rest/device/commands" && !installer)
                   {
                       return req->reply(403);
                   }
                   if (String(path) == "/rest/device/commands" && !installer && auth.user->role != "operator")
                   {
                       return req->reply(403);
                   }
                   auto r = std::make_shared<Request>();
                   r->path = path;
                   r->role = auth.user->role;
                   r->user = auth.user->username;
                   r->mask = auth.user->admin ? 63 : auth.user->channels;
                   r->admin = auth.user->admin;
                   serializeJson(json, r->payload);
                   auto ptr = new std::shared_ptr<Request>(r);
                   if (xQueueSend(queue, &ptr, 0) != pdTRUE)
                   {
                       delete ptr;
                       return req->reply(503);
                   }
                   // A queued operation owns its request until completion; no stack pointers cross
                   // tasks.
                   xSemaphoreTake(r->done, portMAX_DELAY);
                   return req->reply(r->status, "application/json", r->result.c_str());
               });
}

void Actuator::execute(Request &r)
{
    if (r.path == "reset")
    {
        reset();
        return;
    }
    JsonDocument request, response;
    deserializeJson(request, r.payload);
    auto j = request.as<JsonObject>();
    String error;
    String requestId = j["requestId"] | "";
    if (requestId.length() > 64)
    {
        r.status = 422;
        r.result = "{\"error\":\"requestId too long\"}";
        return;
    }
    if (requestId.length())
    {
        for (auto &done : completed)
        {
            if (done.user == r.user && done.id == requestId && uint32_t(millis() - done.at) < 60000)
            {
                if (done.payload != r.path + r.payload)
                {
                    r.status = 409;
                    r.result = "{\"error\":\"requestId reused with different payload\"}";
                }
                else
                {
                    r.status = done.status;
                    r.result = done.result;
                }
                return;
            }
        }
    }
    if (r.path == "/rest/device/config" || r.path == "/rest/protocol/transition")
    {
        Config next;
        if (r.path == "/rest/protocol/transition")
        {
            JsonDocument tmp;
            config.write(tmp.to<JsonObject>());
            tmp["mode"] = j["mode"];
            tmp["revision"] = j["revision"];
            if (!Config::parse(tmp.as<JsonObjectConst>(), next, error))
            {
                r.status = 422;
            }
        }
        else if (!Config::parse(j, next, error))
        {
            r.status = 422;
        }
        if (r.status == 200 && !apply(next, error))
        {
            r.status = 409;
        }
    }
    else if (r.path == "/rest/knx/config")
    {
        if (config.mode != Knx || !knx->configure(j, error))
        {
            r.status = 409;
        }
    }
    else if (r.path == "/rest/knx/programming")
    {
        if (config.mode != Knx || !(NetworkSupport::online()))
        {
            r.status = 409;
            error = "KNX requires an active IPv4 uplink";
        }
        else if (!j["active"].is<bool>())
        {
            r.status = 422;
            error = "Expected active boolean";
        }
        else
        {
            programming(j["active"]);
        }
    }
    else
    {
        String op = j["command"] | "";
        bool installer = r.admin || r.role == "installer";
        if (op == "relay" || op == "pulse")
        {
            int c = j["channel"] | -1;
            if (c < 0 || c > 5 || (!j["value"].is<bool>() && op == "relay"))
            {
                r.status = 422;
                error = "Invalid channel/value";
            }
            else if (!(r.mask & (1 << c)))
            {
                r.status = 403;
                error = "Channel permission denied";
            }
            else if (!relay(c, op == "pulse" ? true : j["value"].as<bool>(), "web", op == "pulse"))
            {
                r.status = 409;
                error = "Channel disabled or blocked";
            }
        }
        else if (op == "all_off" || op == "all_on")
        {
            bool allowed = true;
            for (int i = 0; i < 6; ++i)
            {
                if (config.relays[i].enabled && (!(r.mask & (1 << i)) || blocks[i]))
                {
                    allowed = false;
                }
            }
            if (!allowed)
            {
                r.status = 403;
                error = "Bulk command denied";
            }
            else
            {
                for (int i = 0; i < 6; ++i)
                {
                    if (config.relays[i].enabled)
                    {
                        relay(i, op == "all_on", "web");
                    }
                }
            }
        }
        else if (op == "rgb")
        {
            bool valid = config.rgb;
            for (auto key : {"red", "green", "blue"})
            {
                valid = valid && j[key].is<int>() && j[key].as<int>() >= 0 && j[key].as<int>() <= 255;
            }
            int brightness = j["brightness"] | 10, seconds = j["seconds"] | 5;
            if (!valid || brightness < 0 || brightness > 100 || seconds < 1 || seconds > 30)
            {
                r.status = 422;
                error = "RGB requires enabled output, colors 0–255, brightness 0–100 and duration "
                        "1–30 seconds";
            }
            else
            {
                manualRgb = true;
                manualColor[0] = j["red"];
                manualColor[1] = j["green"];
                manualColor[2] = j["blue"];
                manualBrightness = brightness;
                rgbUntil = millis() + seconds * 1000;
            }
        }
        else if (op == "identify")
        {
            identify();
        }
        else if (op == "tone")
        {
            int hz = j["hz"] | 2000, ms = j["ms"] | 100, duty = j["duty"] | 25;
            if (!config.buzzer || hz < 500 || hz > 4000 || ms < 10 || ms > 2000 || duty < 1 || duty > 50)
            {
                r.status = 422;
                error = "Tone requires enabled buzzer, 500–4000 Hz, 10–2000 ms and 1–50% duty";
            }
            else
            {
                tone(hz, ms, duty);
            }
        }
        else if (op == "acknowledge")
        {
            toneNow = 0;
            toneUntil = 0;
        }
        else if (op == "unblock" && installer)
        {
            int c = j["channel"] | -1;
            if (c < 0 || c > 5)
            {
                r.status = 422;
                error = "Invalid channel";
            }
            else
            {
                blocks[c] = false;
            }
        }
        else if (op == "modbus_window" && installer)
        {
            int seconds = j["seconds"] | 60;
            if (seconds < 0 || seconds > 300)
            {
                r.status = 422;
                error = "Window must be 0–300 seconds";
            }
            else
            {
                modbus->window(seconds, j["peer"] | "rtu");
            }
        }
        else if (op == "factory_reset" && r.admin && j["confirm"] == "ERASE")
        {
            r.result = "{\"ok\":true}";
            xSemaphoreGive(r.done);
            delay(50);
            reset();
            return;
        }
        else
        {
            r.status = 403;
            error = "Unknown or unauthorized command";
        }
    }
    response["ok"] = r.status == 200;
    response["error"] = error;
    response["revision"] = config.revision;
    audit(r.user + " " + r.path + " " + String(r.status));
    if (r.status == 200 && r.path == "/rest/device/commands")
    {
        snapshot(response["state"].to<JsonObject>());
    }
    serializeJson(response, r.result);
    if (requestId.length())
    {
        completed[completedHead++ % 16] = {r.user, requestId, r.path + r.payload, r.result, r.status, millis()};
    }
}

} // namespace actuator
