#pragma once
#include <ArduinoJson.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <map>
#include <string>
#include <vector>

#define FT_ENABLED(x) 1
#define APP_VERSION   "test"
#define pdTRUE        1

class String: public std::string
{
  public:
    using std::string::string;
    String() = default;

    String(const std::string &s) : std::string(s)
    {
    }

    String(int n) : std::string(std::to_string(n))
    {
    }

    bool startsWith(const String &s) const
    {
        return rfind(s, 0) == 0;
    }

    String substring(size_t start) const
    {
        return substr(start);
    }

    int toInt() const
    {
        return std::atoi(c_str());
    }

    size_t write(uint8_t c)
    {
        push_back(char(c));
        return 1;
    }

    size_t write(const uint8_t *p, size_t n)
    {
        append(reinterpret_cast<const char *>(p), n);
        return n;
    }
};

struct TestQueue
{
    size_t capacity;
    std::deque<int> values;
};

using QueueHandle_t = TestQueue *;

inline QueueHandle_t xQueueCreate(int capacity, size_t)
{
    return new TestQueue {size_t(capacity), {}};
}

inline int xQueueSend(QueueHandle_t q, const int *v, int)
{
    if (q->values.size() == q->capacity)
    {
        return 0;
    }
    q->values.push_back(*v);
    return pdTRUE;
}

inline int xQueueReceive(QueueHandle_t q, int *v, int)
{
    if (q->values.empty())
    {
        return 0;
    }
    *v = q->values.front();
    q->values.pop_front();
    return pdTRUE;
}

inline void xQueueReset(QueueHandle_t q)
{
    q->values.clear();
}

struct File
{
    bool valid = true;

    explicit operator bool() const
    {
        return valid;
    }

    void print(const char *)
    {
    }

    void close()
    {
    }
};

struct FS
{
    std::map<std::string, bool> files;

    bool exists(const char *path)
    {
        return files[path];
    }

    File open(const char *path, const char *)
    {
        files[path] = true;
        return {};
    }

    bool remove(const char *path)
    {
        files[path] = false;
        return true;
    }
};

struct MqttSettings
{
    bool enabled = false, homeAssistantDiscovery = false;
};

struct MqttSettingsService
{
    MqttSettings value;
    String status = "esp32/test/status";

    template <class F>
    void read(F f)
    {
        f(value);
    }

    String getStatusTopic()
    {
        return status;
    }
};

struct PsychicMqttClient
{
    struct Message
    {
        String topic, payload;
        int qos;
        bool retain;
    };

    bool online = true, fail = false;
    std::vector<Message> messages;
    std::vector<String> subscriptions;
    std::function<void(bool)> connectCallback, disconnectCallback;
    std::function<void(char *, char *, int, int, bool)> messageCallback;

    bool connected()
    {
        return online;
    }

    template <class F>
    void onConnect(F f)
    {
        connectCallback = f;
    }

    template <class F>
    void onDisconnect(F f)
    {
        disconnectCallback = f;
    }

    template <class F>
    void onMessage(F f)
    {
        messageCallback = f;
    }

    int subscribe(const char *t, int)
    {
        subscriptions.emplace_back(t);
        return fail ? -1 : 1;
    }

    int unsubscribe(const char *)
    {
        return 1;
    }

    int publish(const char *t, int qos, bool retained, const char *p)
    {
        if (fail)
        {
            return -1;
        }
        messages.push_back({t, p, qos, retained});
        return int(messages.size());
    }

    void receive(String t, String p, int retained = 0)
    {
        messageCallback(t.data(), p.data(), retained, 1, false);
    }
};

struct ESP32SvelteKit
{
    MqttSettingsService settings;
    PsychicMqttClient mqtt;

    auto getMqttSettingsService()
    {
        return &settings;
    }

    auto getMqttClient()
    {
        return &mqtt;
    }
};

namespace SettingValue
{
inline String format(const char *)
{
    return "edge_001122334455";
}
} // namespace SettingValue
