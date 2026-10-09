#include "device/XiaozhiMcpCommands.h"
#include "device/XiaozhiMcpPulseCache.h"
#include <XiaozhiMcpProtocol.h>
#include <array>
#include <cassert>
#include <iostream>

struct FakeDevice
{
    struct Relay
    {
        bool enabled = true;
        unsigned pulseMs = 1000;
        std::string name = "Channel";
    };

    struct Config
    {
        unsigned revision = 1, mode = 0;
        bool rgb = true;
        std::array<Relay, 6> relays;
    } config;

    bool outputs[6] {}, blocks[6] {};
    unsigned fault = 0, writes = 0, pulses = 0, identifies = 0;

    bool relay(int c, bool on, const char *source, bool pulse = false)
    {
        assert(std::string(source) == "xiaozhi_mcp");
        if (!config.relays[c].enabled || blocks[c])
        {
            return false;
        }
        ++writes;
        pulses += pulse;
        outputs[c] = on;
        return true;
    }

    void identify()
    {
        ++identifies;
    }
};

int main()
{
    using namespace xiaozhi;
    Endpoint url;
    assert(parseEndpoint("wss://example.invalid:8443/mcp/?token=test", url));
    assert(url.host == "example.invalid" && url.port == 8443 && url.path == "/mcp/?token=test");
    assert(parseEndpoint("wss://example.invalid?token=test", url) && url.path == "/?token=test");
    for (auto bad : {"ws://example.invalid", "wss://a:0/", "wss://a:65536/", "wss://user@a/", "wss://a/#x", "wss://-a/", "wss://a..b/",
                     "wss://a/\n", "wss://[::1]/"})
    {
        assert(!parseEndpoint(bad, url));
    }
    assert(validText("执行器", 64));
    assert(!validText(std::string("\xc0\x80"), 64));
    Settings current, next;
    std::string field;
    JsonDocument edit;
    deserializeJson(
        edit,
        R"({"revision":1,"enabled":true,"alias":" Kitchen ","endpoint":"wss://example.invalid/?token=private-test","channel_mask":3})");
    assert(Settings::update(edit.as<JsonObjectConst>(), current, next, field) == 200);
    assert(next.revision == 2 && next.alias == "Kitchen");
    current = next;
    JsonDocument publicDoc;
    current.write(publicDoc.to<JsonObject>());
    std::string text;
    serializeJson(publicDoc, text);
    assert(text.find("private-test") == std::string::npos && text.find("endpoint_host") != std::string::npos);
    edit["revision"] = 2;
    edit["endpoint"] = "";
    assert(Settings::update(edit.as<JsonObjectConst>(), current, next, field) == 200 && next.endpoint == current.endpoint &&
           next.revision == 2);
    edit["clear_endpoint"] = true;
    assert(Settings::update(edit.as<JsonObjectConst>(), current, next, field) == 422);
    edit["enabled"] = false;
    assert(Settings::update(edit.as<JsonObjectConst>(), current, next, field) == 200 && next.endpoint.empty());
    edit["revision"] = 1;
    assert(Settings::update(edit.as<JsonObjectConst>(), current, next, field) == 409);
    JsonDocument saved;
    current.write(saved.to<JsonObject>(), true);
    assert(Settings::restore(saved.as<JsonObjectConst>(), next) && next == current);
    saved["revision"] = UINT32_MAX;
    assert(Settings::restore(saved.as<JsonObjectConst>(), next) && next.revision == UINT32_MAX && next == current);
    saved["schema_version"] = 2;
    assert(!Settings::restore(saved.as<JsonObjectConst>(), next));

    Protocol protocol("001122334455", "test");
    FakeDevice device;
    unsigned calls = 0;
    uint32_t now = 100;
    auto receive = [&](const std::string &wire)
    {
        return protocol.receive(wire.data(), wire.size(), current, now += 100,
                                [&](Operation op, JsonObjectConst args, JsonObject result)
                                {
                                    ++calls;
                                    return command(device, op, args, current.mask, result);
                                });
    };
    JsonDocument response;
    auto read = [&](const std::string &wire)
    {
        auto output = receive(wire);
        assert(!deserializeJson(response, output));
    };
    read("{");
    assert(response["error"]["code"] == -32700);
    read("{} trailing data");
    assert(response["error"]["code"] == -32700);
    read(std::string("{}\0", 3));
    assert(response["error"]["code"] == -32700);
    read(R"({"jsonrpc":"2.0","id":1,"method":"tools/list"})");
    assert(response["error"]["code"] == -32000);
    read(
        R"({"jsonrpc":"2.0","id":"init","method":"initialize","params":{"protocolVersion":"2024-11-05","clientInfo":{"name":"test"},"capabilities":{}}})");
    assert(response["id"] == "init" && response["result"]["protocolVersion"] == "2024-11-05");
    assert(!protocol.ready());
    assert(receive(R"({"jsonrpc":"2.0","method":"notifications/initialized"})").empty());
    assert(protocol.ready());
    read(R"({"jsonrpc":"2.0","id":"list","method":"tools/list"})");
    assert(response["result"]["tools"].size() == 6);
    const std::string relay =
        R"({"jsonrpc":"2.0","id":"quoted\"id","method":"tools/call","params":{"name":"actuator_set_relay_001122334455","arguments":{"channel":1,"on":true}}})";
    read(relay);
    assert(response["id"] == "quoted\"id" && response["result"]["isError"] == false && device.outputs[0]);
    unsigned writes = device.writes;
    read(relay);
    assert(device.writes == writes && calls == 1);
    auto changed = relay;
    changed.replace(changed.find("true"), 4, "false");
    read(changed);
    assert(response["error"]["code"] == -32600);
    auto notification = relay;
    notification.erase(notification.find("\"id\""), std::string("\"id\":\"quoted\\\"id\",").size());
    assert(receive(notification).empty() && device.writes == writes);
    read(
        R"({"jsonrpc":"2.0","id":9,"method":"tools/call","params":{"name":"actuator_set_relay_001122334455","arguments":{"channel":3,"on":true}}})");
    assert(response["result"]["isError"] == true && !device.outputs[2]);
    read(
        R"({"jsonrpc":"2.0","id":10,"method":"tools/call","params":{"name":"actuator_set_relay_001122334455","arguments":{"channel":1,"on":"true"}}})");
    assert(response["error"]["code"] == -32602);
    device.outputs[1] = true;
    device.blocks[1] = true;
    read(R"({"jsonrpc":"2.0","id":11,"method":"tools/call","params":{"name":"actuator_all_off_001122334455","arguments":{}}})");
    assert(response["result"]["isError"] == true && device.outputs[0] && device.outputs[1]);
    device.blocks[1] = false;
    device.outputs[2] = true;
    read(R"({"jsonrpc":"2.0","id":12,"method":"tools/call","params":{"name":"actuator_all_off_001122334455","arguments":{}}})");
    assert(!device.outputs[0] && !device.outputs[1] && device.outputs[2]);
    current.mask = 0;
    read(R"({"jsonrpc":"2.0","id":13,"method":"tools/list"})");
    assert(response["result"]["tools"].size() == 3);
    read(std::string(9000, ' '));
    assert(response["error"]["code"] == -32600);
    protocol.reset();
    assert(!protocol.ready());

    FakeDevice channels;
    JsonDocument channelArgs, channelResult;
    for (unsigned c = 1; c <= 6; ++c)
    {
        channelArgs["channel"] = c;
        channelArgs["on"] = true;
        auto args = channelArgs.as<JsonObjectConst>();
        auto result = channelResult.to<JsonObject>();
        const auto mask = uint8_t(1 << (c - 1));
        assert(command(channels, Operation::Relay, args, mask, result) && channels.outputs[c - 1]);
        channels.blocks[c - 1] = true;
        assert(!command(channels, Operation::Relay, args, mask, result));
        channels.blocks[c - 1] = false;
        channels.config.relays[c - 1].enabled = false;
        assert(!command(channels, Operation::Pulse, args, mask, result));
        channels.config.relays[c - 1].enabled = true;
        assert(command(channels, Operation::Pulse, args, mask, result) && result["duration_ms"] == 1000);
    }
    channels.config.rgb = false;
    assert(!command(channels, Operation::Identify, channelArgs.as<JsonObjectConst>(), 63, channelResult.to<JsonObject>()));
    assert(channels.pulses == 6 && channels.identifies == 0);

    PulseCache pulses;
    JsonDocument pulseResult;
    unsigned pulsesApplied = 0;
    auto pulse = [&](const std::string &id, const std::string &fingerprint, uint32_t at)
    {
        auto out = pulseResult.to<JsonObject>();
        return pulses.run(id,
                          fingerprint,
                          at,
                          out,
                          [&]()
                          {
                              ++pulsesApplied;
                              out["on"] = true;
                              return true;
                          });
    };
    assert(pulse("first", "revision1:channel1", 100));
    assert(pulse("first", "revision1:channel1", 101) && pulsesApplied == 1 && pulseResult["on"] == true);
    assert(!pulse("first", "revision2:channel1", 102) && pulseResult["error"] == "operation_id_reused");
    for (unsigned i = 1; i < 16; ++i)
    {
        assert(pulse(std::to_string(i), "revision1:channel1", 100));
    }
    assert(!pulse("overflow", "revision1:channel1", 59999) && pulseResult["error"] == "pulse_retry_capacity" && pulsesApplied == 16);
    assert(pulse("first", "revision1:channel1", 60099) && pulsesApplied == 16);
    assert(pulse("overflow", "revision1:channel1", 60100) && pulsesApplied == 17);
    PulseCache wrap;
    auto out = pulseResult.to<JsonObject>();
    assert(wrap.run("wrap",
                    "same",
                    UINT32_MAX - 100,
                    out,
                    [&]()
                    {
                        ++pulsesApplied;
                        return true;
                    }));
    assert(wrap.run("wrap",
                    "same",
                    100,
                    out,
                    [&]()
                    {
                        ++pulsesApplied;
                        return true;
                    }) &&
           pulsesApplied == 18);
    std::cout << "Xiaozhi MCP protocol/settings/actuator tests passed\n";
}
