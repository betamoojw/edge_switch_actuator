#pragma once
// Project-native MCP implementation. No WLED or vendor wrapper source is included.
#include <ArduinoJson.h>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>

namespace xiaozhi
{
constexpr size_t MaxMessage = 8192, MaxResponse = 16384;

// Limit actual ArduinoJson allocations, not just the serialized input length.
class JsonBudget: public ArduinoJson::Allocator
{
    size_t used = 0;
    static constexpr size_t Limit = 49152;

  public:
    void *allocate(size_t size) override
    {
        if (size > Limit - used)
        {
            return nullptr;
        }
        auto p = static_cast<size_t *>(std::malloc(size + sizeof(size_t)));
        if (!p)
        {
            return nullptr;
        }
        *p = size;
        used += size;
        return p + 1;
    }

    void deallocate(void *ptr) override
    {
        if (!ptr)
        {
            return;
        }
        auto p = static_cast<size_t *>(ptr) - 1;
        used -= *p;
        std::free(p);
    }

    void *reallocate(void *ptr, size_t size) override
    {
        if (!ptr)
        {
            return allocate(size);
        }
        auto p = static_cast<size_t *>(ptr) - 1;
        const size_t old = *p;
        if (size > Limit - (used - old))
        {
            return nullptr;
        }
        auto next = static_cast<size_t *>(std::realloc(p, size + sizeof(size_t)));
        if (!next)
        {
            return nullptr;
        }
        used = used - old + size;
        *next = size;
        return next + 1;
    }
};

struct Endpoint
{
    std::string host, path;
    uint16_t port = 443;
};

bool parseEndpoint(const std::string &url, Endpoint &out);
bool validText(const std::string &text, size_t limit);

struct Settings
{
    uint32_t revision = 1;
    bool enabled = false;
    std::string alias = "Switching Actuator", endpoint;
    uint8_t mask = 0;
    void write(JsonObject out, bool secret = false) const;
    // Returns HTTP status, writes only a complete validated candidate.
    static int update(JsonObjectConst input, const Settings &current, Settings &candidate, std::string &field);
    static bool restore(JsonObjectConst input, Settings &settings);
    bool operator==(const Settings &other) const;
};

enum class Operation
{
    Status,
    Alias,
    Relay,
    Pulse,
    AllOff,
    Identify
};
void tools(JsonArray out, const std::string &id, const Settings &settings);

class Protocol
{
  public:
    using Execute = std::function<bool(Operation, JsonObjectConst, JsonObject)>;

    Protocol(std::string id, std::string version) : id(std::move(id)), version(std::move(version))
    {
    }

    void reset();

    bool ready() const
    {
        return initialized;
    }

    // Empty response means notification; notification tool calls never execute.
    std::string receive(const char *data, size_t length, const Settings &settings, uint32_t now, Execute execute);

  private:
    std::string id, version;
    bool initializing = false, initialized = false;

    struct Completed
    {
        std::string id, request, response;
    } completed[8];

    size_t head = 0;
    uint32_t refillAt = 0;
    unsigned tokens = 4;
};
} // namespace xiaozhi
