#include "XiaozhiMcpProtocol.h"
#include <algorithm>
#include <cctype>

namespace xiaozhi
{
namespace
{
struct MessageReader
{
    const char *data;
    size_t length, offset = 0;

    int read()
    {
        return offset < length ? static_cast<unsigned char>(data[offset++]) : -1;
    }

    size_t readBytes(char *out, size_t count)
    {
        count = std::min(count, length - offset);
        if (count)
        {
            std::memcpy(out, data + offset, count);
        }
        offset += count;
        return count;
    }
};
} // namespace

bool validText(const std::string &s, size_t limit)
{
    if (s.empty() || s.size() > limit)
    {
        return false;
    }
    // Validate UTF-8, excluding control characters and overlong/surrogate encodings.
    for (size_t i = 0; i < s.size();)
    {
        uint8_t c = s[i++];
        if (c < 32 || c == 127)
        {
            return false;
        }
        if (c < 128)
        {
            continue;
        }
        unsigned n;
        uint32_t value, minimum;
        if (c >= 0xc2 && c <= 0xdf)
        {
            n = 1;
            value = c & 31;
            minimum = 128;
        }
        else if (c >= 0xe0 && c <= 0xef)
        {
            n = 2;
            value = c & 15;
            minimum = 2048;
        }
        else if (c >= 0xf0 && c <= 0xf4)
        {
            n = 3;
            value = c & 7;
            minimum = 65536;
        }
        else
        {
            return false;
        }
        if (i + n > s.size())
        {
            return false;
        }
        while (n--)
        {
            uint8_t b = s[i++];
            if ((b & 0xc0) != 0x80)
            {
                return false;
            }
            value = (value << 6) | (b & 63);
        }
        if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
        {
            return false;
        }
    }
    return true;
}

bool parseEndpoint(const std::string &url, Endpoint &out)
{
    if (url.size() > 2048 || url.compare(0, 6, "wss://") != 0)
    {
        return false;
    }
    for (unsigned char c : url)
    {
        if (c <= 32 || c >= 127 || c == '#' || c == '\\')
        {
            return false;
        }
    }
    const size_t end = url.find_first_of("/?", 6);
    const std::string authority = url.substr(6, end == std::string::npos ? end : end - 6);
    if (authority.empty() || authority.find('@') != std::string::npos)
    {
        return false;
    }
    const auto colon = authority.find(':');
    out = Endpoint();
    out.host = authority.substr(0, colon);
    // This project currently supports IPv4/DNS uplinks. Reject IPv6 literals explicitly.
    if (out.host.empty() || out.host.size() > 253 || out.host.front() == '.' || out.host.back() == '.')
    {
        return false;
    }
    size_t label = 0;
    for (size_t i = 0; i < out.host.size(); ++i)
    {
        const char c = out.host[i];
        if (c == '.')
        {
            if (!label || out.host[i - 1] == '-')
            {
                return false;
            }
            label = 0;
        }
        else
        {
            if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '-') || (!label && c == '-') || ++label > 63)
            {
                return false;
            }
        }
    }
    if (!label || out.host.back() == '-')
    {
        return false;
    }
    if (colon != std::string::npos)
    {
        const std::string port = authority.substr(colon + 1);
        if (port.empty() || port.size() > 5)
        {
            return false;
        }
        unsigned value = 0;
        for (char c : port)
        {
            if (c < '0' || c > '9')
            {
                return false;
            }
            value = value * 10 + c - '0';
        }
        if (!value || value > 65535)
        {
            return false;
        }
        out.port = value;
    }
    out.path = end == std::string::npos ? "/" : (url[end] == '?' ? "/" + url.substr(end) : url.substr(end));
    return true;
}

void Settings::write(JsonObject out, bool secret) const
{
    out["schema_version"] = 1;
    out["revision"] = revision;
    out["enabled"] = enabled;
    out["alias"] = alias;
    out["channel_mask"] = mask;
    if (secret)
    {
        out["endpoint"] = endpoint;
    }
    else
    {
        Endpoint parsed;
        out["endpoint_configured"] = !endpoint.empty();
        out["endpoint_host"] = parseEndpoint(endpoint, parsed) ? parsed.host : "";
    }
}

bool Settings::operator==(const Settings &s) const
{
    return enabled == s.enabled && alias == s.alias && endpoint == s.endpoint && mask == s.mask;
}

int Settings::update(JsonObjectConst in, const Settings &current, Settings &next, std::string &field)
{
    for (auto pair : in)
    {
        const std::string key = pair.key().c_str();
        if (key != "revision" && key != "enabled" && key != "alias" && key != "channel_mask" && key != "endpoint" &&
            key != "clear_endpoint")
        {
            field = "fields";
            return 422;
        }
    }
    field = "revision";
    if (!in["revision"].is<uint32_t>())
    {
        return 422;
    }
    if (in["revision"].as<uint32_t>() != current.revision)
    {
        return 409;
    }
    field = "enabled";
    if (!in["enabled"].is<bool>())
    {
        return 422;
    }
    field = "alias";
    if (!in["alias"].is<std::string>())
    {
        return 422;
    }
    next = current;
    next.alias = in["alias"].as<std::string>();
    auto first = next.alias.find_first_not_of(' '), last = next.alias.find_last_not_of(' ');
    next.alias = first == std::string::npos ? "" : next.alias.substr(first, last - first + 1);
    if (!validText(next.alias, 64))
    {
        return 422;
    }
    field = "channel_mask";
    if (!in["channel_mask"].is<unsigned>() || in["channel_mask"].as<unsigned>() > 63)
    {
        return 422;
    }
    next.mask = in["channel_mask"];
    next.enabled = in["enabled"];
    field = "endpoint";
    if (!in["endpoint"].isUnbound() && !in["endpoint"].is<std::string>())
    {
        return 422;
    }
    if (!in["clear_endpoint"].isUnbound() && !in["clear_endpoint"].is<bool>())
    {
        return 422;
    }
    const auto endpoint = in["endpoint"].is<std::string>() ? in["endpoint"].as<std::string>() : "";
    const bool clear = in["clear_endpoint"] | false;
    if (clear && !endpoint.empty())
    {
        return 422;
    }
    if (clear)
    {
        next.endpoint.clear();
    }
    if (!endpoint.empty())
    {
        next.endpoint = endpoint;
    }
    Endpoint parsed;
    if ((!next.endpoint.empty() && !parseEndpoint(next.endpoint, parsed)) || (next.enabled && next.endpoint.empty()))
    {
        return 422;
    }
    field.clear();
    if (!(next == current))
    {
        if (current.revision == UINT32_MAX)
        {
            field = "revision";
            return 409;
        }
        next.revision = current.revision + 1;
    }
    return 200;
}

bool Settings::restore(JsonObjectConst in, Settings &out)
{
    if (in["schema_version"] != 1 || !in["revision"].is<uint32_t>() || in["revision"] == 0 || !in["endpoint"].is<std::string>())
    {
        return false;
    }
    JsonDocument edit;
    for (auto key : {"revision", "enabled", "alias", "channel_mask", "endpoint"})
    {
        edit[key] = in[key];
    }
    Settings baseline;
    // Validate independently of the saved revision; UINT32_MAX is readable even
    // though further changes require an explicit reset.
    edit["revision"] = baseline.revision;
    std::string field;
    if (update(edit.as<JsonObjectConst>(), baseline, out, field) != 200)
    {
        return false;
    }
    out.revision = in["revision"];
    return true;
}

static const char *names[] = {"actuator_status_",      "actuator_get_alias_", "actuator_set_relay_",
                              "actuator_pulse_relay_", "actuator_all_off_",   "actuator_identify_"};
static const char *descriptions[] = {"Read exposed relay status",   "Read device alias",
                                     "Set a relay on or off",       "Pulse a relay for its configured duration",
                                     "Turn off all exposed relays", "Identify the device for five seconds"};

void tools(JsonArray out, const std::string &id, const Settings &settings)
{
    for (unsigned i = 0; i < 6; ++i)
    {
        if (!settings.mask && i >= 2 && i <= 4)
        {
            continue;
        }
        auto tool = out.add<JsonObject>();
        tool["name"] = names[i] + id;
        tool["description"] = settings.alias + ": " + descriptions[i];
        auto schema = tool["inputSchema"].to<JsonObject>();
        schema["type"] = "object";
        schema["additionalProperties"] = false;
        auto props = schema["properties"].to<JsonObject>();
        auto required = schema["required"].to<JsonArray>();
        if (i == 2 || i == 3)
        {
            auto channel = props["channel"].to<JsonObject>();
            channel["type"] = "integer";
            auto allowed = channel["enum"].to<JsonArray>();
            for (int c = 0; c < 6; ++c)
            {
                if (settings.mask & (1 << c))
                {
                    allowed.add(c + 1);
                }
            }
            required.add("channel");
            if (i == 2)
            {
                props["on"]["type"] = "boolean";
                required.add("on");
            }
            else
            {
                props["request_id"]["type"] = "string";
                props["request_id"]["minLength"] = 1;
                props["request_id"]["maxLength"] = 64;
                required.add("request_id");
            }
        }
    }
}

void Protocol::reset()
{
    initialized = initializing = false;
    head = 0;
    tokens = 4;
    refillAt = 0;
    for (auto &entry : completed)
    {
        entry = {};
    }
}

std::string Protocol::receive(const char *data, size_t length, const Settings &settings, uint32_t now, Execute execute)
{
    JsonBudget budget;
    JsonDocument input(&budget), output(&budget);
    output["jsonrpc"] = "2.0";
    output["id"] = nullptr;
    auto finish = [&]() -> std::string
    {
        if (output.overflowed() || measureJson(output) > MaxResponse)
        {
            return "{\"jsonrpc\":\"2.0\",\"id\":null,\"error\":{\"code\":-32603,\"message\":\"Response capacity exceeded\"}}";
        }
        std::string wire;
        serializeJson(output, wire);
        return wire;
    };
    auto error = [&](int code, const char *message)
    {
        output.remove("result");
        output["error"]["code"] = code;
        output["error"]["message"] = message;
        return finish();
    };
    if (length > MaxMessage)
    {
        return error(-32600, "Message too large");
    }
    MessageReader reader {data, length};
    if (deserializeJson(input, reader, DeserializationOption::NestingLimit(8)))
    {
        return error(-32700, "Invalid JSON");
    }
    while (reader.offset < length)
    {
        const int c = reader.read();
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
        {
            return error(-32700, "Invalid JSON");
        }
    }
    if (!input.is<JsonObject>() || input["jsonrpc"] != "2.0" || !input["method"].is<std::string>())
    {
        return error(-32600, "Invalid request");
    }
    const bool notification = input["id"].isUnbound();
    const std::string method = input["method"].as<std::string>();
    if (notification)
    {
        if (method == "notifications/initialized" && initializing)
        {
            initialized = true;
        }
        return "";
    }
    if (!(input["id"].is<int64_t>() || input["id"].is<std::string>()) ||
        (input["id"].is<std::string>() && input["id"].as<std::string>().size() > 128))
    {
        return error(-32600, "Invalid request id");
    }
    output["id"] = input["id"];
    if (method == "ping")
    {
        output["result"].to<JsonObject>();
        return finish();
    }
    if (method == "initialize")
    {
        if (initializing || initialized)
        {
            return error(-32600, "Already initialized");
        }
        if (!input["params"]["protocolVersion"].is<std::string>() || !input["params"]["clientInfo"].is<JsonObject>() ||
            !input["params"]["capabilities"].is<JsonObject>())
        {
            return error(-32602, "Invalid initialization parameters");
        }
        output["result"]["protocolVersion"] = "2024-11-05";
        output["result"]["capabilities"]["tools"]["listChanged"] = false;
        output["result"]["serverInfo"]["name"] = "Edge Switching Actuator";
        output["result"]["serverInfo"]["version"] = version;
        initializing = true;
        return finish();
    }
    if (!initialized)
    {
        return error(-32000, "Initialization required");
    }
    if (method == "tools/list")
    {
        if (!input["params"].isUnbound() && !input["params"].is<JsonObject>())
        {
            return error(-32602, "Invalid parameters");
        }
        if (!input["params"]["cursor"].isUnbound())
        {
            return error(-32602, "Pagination cursor unsupported");
        }
        tools(output["result"]["tools"].to<JsonArray>(), id, settings);
        return finish();
    }
    if (method != "tools/call")
    {
        return error(-32601, "Method not found");
    }
    // MCP request metadata is independent of a tool's input schema. Arguments
    // are optional for zero-input tools (e.g. Xiaozhi's status calls), so do not
    // constrain the envelope to exactly name + arguments.
    if (!input["params"].is<JsonObject>() || !input["params"]["name"].is<std::string>() ||
        (!input["params"]["arguments"].isUnbound() && !input["params"]["arguments"].is<JsonObject>()) ||
        (!input["params"]["_meta"].isUnbound() && !input["params"]["_meta"].is<JsonObject>()))
    {
        return error(-32602, "Invalid tool parameters");
    }
    if (input["params"]["arguments"].isUnbound())
    {
        input["params"]["arguments"].to<JsonObject>();
        if (input.overflowed())
        {
            return error(-32603, "Request capacity exceeded");
        }
    }
    int index = -1;
    for (int i = 0; i < 6; ++i)
    {
        if (input["params"]["name"].as<std::string>() == names[i] + id)
        {
            index = i;
        }
    }
    if (index < 0 || (!settings.mask && index >= 2 && index <= 4))
    {
        return error(-32602, "Unknown tool");
    }
    auto args = input["params"]["arguments"].as<JsonObjectConst>();
    if (index == 2 || index == 3)
    {
        if (args.size() != 2 || !args["channel"].is<unsigned>() || args["channel"].as<unsigned>() < 1 || args["channel"].as<unsigned>() > 6)
        {
            return error(-32602, "Invalid channel");
        }
        if (index == 2 && !args["on"].is<bool>())
        {
            return error(-32602, "Expected on boolean");
        }
        if (index == 3 && (!args["request_id"].is<std::string>() || !validText(args["request_id"].as<std::string>(), 64)))
        {
            return error(-32602, "Invalid operation id");
        }
    }
    else if (args.size())
    {
        return error(-32602, "Unexpected arguments");
    }
    std::string key, request;
    serializeJson(input["id"], key);
    serializeJson(input["params"], request);
    for (auto &entry : completed)
    {
        if (entry.id == key)
        {
            return entry.request == request ? entry.response : error(-32600, "Request id reused");
        }
    }
    const uint32_t elapsed = now - refillAt;
    if (elapsed >= 100)
    {
        tokens = std::min(uint32_t(4), uint32_t(tokens) + elapsed / 100);
        refillAt = now;
    }
    if (!tokens)
    {
        return error(-32000, "Rate limit exceeded");
    }
    --tokens;
    JsonDocument result(&budget);
    const bool ok = execute(static_cast<Operation>(index), args, result.to<JsonObject>());
    std::string text;
    serializeJson(result, text);
    auto content = output["result"]["content"].to<JsonArray>().add<JsonObject>();
    content["type"] = "text";
    content["text"] = text;
    output["result"]["isError"] = !ok || result.overflowed();
    auto response = finish();
    completed[head++ % 8] = {key, request, response};
    return response;
}
} // namespace xiaozhi
