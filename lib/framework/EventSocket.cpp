#include <EventSocket.h>
#include <atomic>

SemaphoreHandle_t clientSubscriptionsMutex = xSemaphoreCreateMutex();

EventSocket::EventSocket(PsychicHttpServer *server, SecurityManager *securityManager, AuthenticationPredicate authenticationPredicate)
    : _server(server), _securityManager(securityManager), _authenticationPredicate(authenticationPredicate)
{
}

void EventSocket::begin()
{
    _socket.setFilter(_securityManager->filterRequest(_authenticationPredicate));
    _socket.onOpen((std::bind(&EventSocket::onWSOpen, this, std::placeholders::_1)));
    _socket.onClose(std::bind(&EventSocket::onWSClose, this, std::placeholders::_1));
    _socket.onFrame(std::bind(&EventSocket::onFrame, this, std::placeholders::_1, std::placeholders::_2));
    _server->on(EVENT_SERVICE_PATH, &_socket);

    ESP_LOGV(SVK_TAG, "Registered event socket endpoint: %s", EVENT_SERVICE_PATH);
}

void EventSocket::registerEvent(String event)
{
    if (!isEventValid(event))
    {
        ESP_LOGD(SVK_TAG, "Registering event: %s", event.c_str());
        events.push_back(event);
    }
    else
    {
        ESP_LOGW(SVK_TAG, "Event already registered: %s", event.c_str());
    }
}

void EventSocket::onWSOpen(PsychicWebSocketClient *client)
{
    ESP_LOGI(SVK_TAG, "ws[%s][%u] connect", client->remoteIP().toString().c_str(), client->socket());
}

void EventSocket::onWSClose(PsychicWebSocketClient *client)
{
    xSemaphoreTake(clientSubscriptionsMutex, portMAX_DELAY);
    for (auto &event_subscriptions : client_subscriptions)
    {
        event_subscriptions.second.remove(client->socket());
    }
    xSemaphoreGive(clientSubscriptionsMutex);
    ESP_LOGI(SVK_TAG, "ws[%s][%u] disconnect", client->remoteIP().toString().c_str(), client->socket());
}

esp_err_t EventSocket::onFrame(PsychicWebSocketRequest *request, httpd_ws_frame *frame)
{
    ESP_LOGV(SVK_TAG, "ws[%s][%u] opcode[%d]", request->client()->remoteIP().toString().c_str(), request->client()->socket(), frame->type);

    if (frame->len > 8192)
    {
        return ESP_FAIL;
    }
    JsonDocument doc;
#if FT_ENABLED(EVENT_USE_JSON)
    if (frame->type == HTTPD_WS_TYPE_TEXT)
    {
        ESP_LOGV(SVK_TAG, "ws[%s][%u] request: %s", request->client()->remoteIP().toString().c_str(), request->client()->socket(),
                 (char *) frame->payload);

        DeserializationError error = deserializeJson(doc, (char *) frame->payload, frame->len);
#else
    if (frame->type == HTTPD_WS_TYPE_BINARY)
    {
        ESP_LOGV(SVK_TAG, "ws[%s][%u] request: %s", request->client()->remoteIP().toString().c_str(), request->client()->socket(),
                 (char *) frame->payload);

        DeserializationError error = deserializeMsgPack(doc, (char *) frame->payload, frame->len);
#endif

        if (!error && doc.is<JsonObject>())
        {
            String event = doc["event"];
            if (event == "ping")
            {
                // Reply on this authenticated connection even when telemetry is idle.
                JsonDocument pong;
                pong["event"] = "pong";
                String output;
#if FT_ENABLED(EVENT_USE_JSON)
                serializeJson(pong, output);
                return request->client()->sendMessage(HTTPD_WS_TYPE_TEXT, output.c_str(), output.length());
#else
                serializeMsgPack(pong, output);
                return request->client()->sendMessage(HTTPD_WS_TYPE_BINARY, output.c_str(), output.length());
#endif
            }
            else if (event == "subscribe")
            {
                // only subscribe to events that are registered
                if (isEventValid(doc["data"].as<String>()))
                {
                    xSemaphoreTake(clientSubscriptionsMutex, portMAX_DELAY);
                    auto &members = client_subscriptions[doc["data"]];
                    members.remove(request->client()->socket());
                    members.push_back(request->client()->socket());
                    xSemaphoreGive(clientSubscriptionsMutex);
                    handleSubscribeCallbacks(doc["data"], String(request->client()->socket()));
                }
                else
                {
                    ESP_LOGW(SVK_TAG, "Client tried to subscribe to unregistered event: %s", doc["data"].as<String>().c_str());
                }
            }
            else if (event == "unsubscribe")
            {
                xSemaphoreTake(clientSubscriptionsMutex, portMAX_DELAY);
                client_subscriptions[doc["data"]].remove(request->client()->socket());
                xSemaphoreGive(clientSubscriptionsMutex);
            }
            else
            {
                if (!isEventValid(event) || !doc["data"].is<JsonObject>())
                {
                    return ESP_FAIL;
                }
                JsonObject jsonObject = doc["data"].as<JsonObject>();
                handleEventCallbacks(event, jsonObject, request->client()->socket());
            }
            return ESP_OK;
        }
        ESP_LOGW(SVK_TAG, "Error[%d] parsing JSON: %s", error, (char *) frame->payload);
    }
    return ESP_OK;
}

void EventSocket::emitEvent(String event, JsonObject &jsonObject, const char *originId, bool onlyToSameOrigin)
{
    // Only process valid events
    if (!isEventValid(String(event)))
    {
        ESP_LOGW(SVK_TAG, "Method tried to emit unregistered event: %s", event);
        return;
    }

    // Run sends on the HTTP task, never under the actuator or subscription lock.
    // Bound pending work so a slow peer cannot accumulate telemetry indefinitely.
    static std::atomic<unsigned> pending{0};
    if (pending.fetch_add(1) >= 8)
    {
        --pending;
        return;
    }
    struct Delivery
    {
        EventSocket *owner;
        String event;
        std::vector<uint8_t> payload;
        std::list<int> recipients;
        std::atomic<unsigned> *pending;
    };
    auto *delivery = new Delivery{this, event, {}, {}, &pending};
    int origin = originId[0] ? atoi(originId) : -1;
    xSemaphoreTake(clientSubscriptionsMutex, portMAX_DELAY);
    for (int id : client_subscriptions[event])
    {
        if (onlyToSameOrigin ? id == origin : id != origin) delivery->recipients.push_back(id);
    }
    xSemaphoreGive(clientSubscriptionsMutex);
    JsonDocument doc;
    doc["event"] = event;
    doc["data"] = jsonObject;
#if FT_ENABLED(EVENT_USE_JSON)
    delivery->payload.resize(measureJson(doc));
    serializeJson(doc, delivery->payload.data(), delivery->payload.size());
#else
    delivery->payload.resize(measureMsgPack(doc));
    serializeMsgPack(doc, delivery->payload.data(), delivery->payload.size());
#endif
    auto deliver = [](void *arg) {
        auto *job = static_cast<Delivery *>(arg);
        for (int id : job->recipients)
        {
            xSemaphoreTake(clientSubscriptionsMutex, portMAX_DELAY);
            const auto &members = job->owner->client_subscriptions[job->event];
            bool subscribed = std::find(members.begin(), members.end(), id) != members.end();
            xSemaphoreGive(clientSubscriptionsMutex);
            auto *client = subscribed ? job->owner->_socket.getClient(id) : nullptr;
            if (!client) continue;
#if FT_ENABLED(EVENT_USE_JSON)
            client->sendMessage(HTTPD_WS_TYPE_TEXT, job->payload.data(), job->payload.size());
#else
            client->sendMessage(HTTPD_WS_TYPE_BINARY, job->payload.data(), job->payload.size());
#endif
        }
        --(*job->pending);
        delete job;
    };
    if (delivery->recipients.empty() || httpd_queue_work(_server->server, deliver, delivery) != ESP_OK)
    {
        --pending;
        delete delivery;
    }
}

void EventSocket::handleEventCallbacks(String event, JsonObject &jsonObject, int originId)
{
    for (auto &callback : event_callbacks[event])
    {
        callback(jsonObject, originId);
    }
}

void EventSocket::handleSubscribeCallbacks(String event, const String &originId)
{
    for (auto &callback : subscribe_callbacks[event])
    {
        callback(originId);
    }
}

void EventSocket::onEvent(String event, EventCallback callback)
{
    if (!isEventValid(event))
    {
        ESP_LOGW(SVK_TAG, "Method tried to register unregistered event: %s", event.c_str());
        return;
    }
    event_callbacks[event].push_back(callback);
}

void EventSocket::onSubscribe(String event, SubscribeCallback callback)
{
    if (!isEventValid(event))
    {
        ESP_LOGW(SVK_TAG, "Method tried to subscribe to unregistered event: %s", event.c_str());
        return;
    }
    subscribe_callbacks[event].push_back(callback);
    ESP_LOGI(SVK_TAG, "onSubscribe for event: %s", event.c_str());
}

bool EventSocket::isEventValid(String event)
{
    return std::find(events.begin(), events.end(), event) != events.end();
}

unsigned int EventSocket::getConnectedClients()
{
    return (unsigned int) _socket.getClientList().size();
}
