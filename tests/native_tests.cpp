#include "device/Gesture.h"
#include "device/HomeAssistantCommand.h"
#include "protocols/ModbusPdu.h"
#include <array>
#include <assert.h>
#include <stdio.h>
#include <vector>

struct Backend
{
    int accepted = 0, rejected = 0, writes = 0;
    bool values[6] = {};

    void accept()
    {
        ++accepted;
    }

    void reject()
    {
        ++rejected;
    }

    const char *version()
    {
        return "test";
    }

    bool readBit(bool, uint16_t address, bool &value, int)
    {
        if (address >= 6)
        {
            return false;
        }
        value = values[address];
        return true;
    }

    bool readReg(bool, uint16_t address, uint16_t &value, int)
    {
        if (address >= 6)
        {
            return false;
        }
        value = 0xab00 + address;
        return true;
    }

    uint8_t writeBits(uint16_t address, uint16_t count, const uint8_t *bytes, int)
    {
        if (uint32_t(address) + count > 6)
        {
            return 2;
        }
        for (int i = 0; i < count; ++i)
        {
            values[address + i] = bytes[i / 8] & (1 << (i % 8));
        }
        ++writes;
        return 0;
    }

    uint8_t writeRegs(uint16_t address, uint16_t count, const uint8_t *, int)
    {
        if (uint32_t(address) + count > 6)
        {
            return 2;
        }
        ++writes;
        return 0;
    }
};

static void protocolTests()
{
    Backend backend;
    std::array<uint8_t, 260> out;
    auto send = [&](std::initializer_list<uint8_t> bytes, bool serial = true, bool broadcast = false)
    {
        std::vector<uint8_t> request(bytes);
        out.fill(0x55);
        return actuator::wire::processPdu(backend, request.data(), request.size(), out.data(), serial, 0, broadcast);
    };
    assert(send({5, 0, 0, 0xff, 0}) == 5 && backend.values[0]);
    assert(send({1, 0, 0, 0, 6}) == 3 && out[2] == 1);
    assert(send({5, 0, 0, 0x55, 0}) == 2 && out[1] == 3 && backend.values[0]);
    assert(send({15, 0, 0, 0, 6, 1, 0x2a}) == 5 && !backend.values[0] && backend.values[1] && backend.values[5]);
    int writes = backend.writes;
    assert(send({15, 0, 0, 0, 6, 2, 0x3f}) == 2 && out[1] == 3 && backend.writes == writes);
    assert(send({15, 0, 5, 0, 2, 1, 3}) == 2 && out[1] == 2 && backend.writes == writes);
    assert(send({16, 0, 0, 0, 2, 4, 0, 1, 0, 2}) == 5);
    assert(send({16, 0, 0, 0, 2, 2, 0, 1}) == 2 && out[1] == 3);
    assert(send({3, 0, 0, 0, 2}) == 6 && out[2] == 0xab && out[3] == 0 && out[5] == 1);
    assert(send({3, 0xff, 0xff, 0, 2}) == 2 && out[1] == 3);
    assert(send({1, 0, 0, 0, 0}) == 2 && out[1] == 3);
    assert(send({8, 0, 0, 0x12, 0x34}) == 5 && out[4] == 0x34);
    assert(send({8, 0, 0, 0x12, 0x34}, false) == 2 && out[1] == 1);
    assert(send({43, 14, 1, 0}) > 7 && out[6] == 3);
    assert(send({43, 14, 1, 3}) == 2 && out[1] == 2);
    assert(send({43, 14, 2, 0}) == 2 && out[1] == 3);
    writes = backend.writes;
    assert(send({5, 0, 1, 0, 0}, true, true) == 0 && backend.writes == writes + 1);
    writes = backend.writes;
    assert(send({5, 0, 16, 0xff, 0}, true, true) == 0 && backend.writes == writes);
    assert(send({1, 0, 0, 0, 1}, true, true) == 0);
    // Exercise every short/truncated PDU, with canaries around the maximum response.
    for (int function = 0; function < 256; ++function)
    {
        for (size_t length = 0; length < 6; ++length)
        {
            uint8_t input[8] = {uint8_t(function), 0, 0, 0, 1, 1};
            std::array<uint8_t, 270> buffer;
            buffer.fill(0xcc);
            auto size = actuator::wire::processPdu(backend, input, length, buffer.data() + 5, true, 0);
            assert(size <= 253);
            for (int i = 0; i < 5; ++i)
            {
                assert(buffer[i] == 0xcc && buffer[265 + i] == 0xcc);
            }
        }
    }
}

static void gestureTests()
{
    Gesture g;
    uint32_t now = 0;
    std::vector<int> events;
    auto step = [&](bool down, int duration)
    {
        for (int i = 0; i < duration; i += 5)
        {
            auto e = g.update(down, now);
            now += 5;
            if (e)
            {
                events.push_back(e);
            }
        }
    };
    step(false, 100);
    step(true, 60);
    step(false, 500);
    assert((events == std::vector<int> {Gesture::Single}));
    events.clear();
    step(true, 60);
    step(false, 100);
    step(true, 60);
    step(false, 500);
    assert((events == std::vector<int> {Gesture::Double}));
    events.clear();
    for (int i = 0; i < 3; ++i)
    {
        step(true, 60);
        step(false, 100);
    }
    step(false, 500);
    assert((events == std::vector<int> {Gesture::Triple}));
    events.clear();
    for (int i = 0; i < 4; ++i)
    {
        step(true, 60);
        step(false, 100);
    }
    step(false, 500);
    assert(events.empty());
    for (int i = 0; i < 10; ++i)
    {
        step(true, 10);
        step(false, 10);
    }
    step(false, 500);
    assert(events.empty());
    step(true, 6000);
    assert(g.armed(now));
    step(false, 500);
    assert(events.empty());
    step(true, 11000);
    step(true, 5000);
    step(false, 500);
    assert((events == std::vector<int> {Gesture::Reset}));
    events.clear();
    Gesture held;
    for (uint32_t t = 0; t < 15000; t += 5)
    {
        assert(held.update(true, t) == Gesture::None);
    }
    // Same click sequence across uint32_t uptime wrap.
    Gesture wrap;
    uint32_t t = 0xffffff00;
    for (int i = 0; i < 100; ++i)
    {
        wrap.update(false, t);
        t += 5;
    }
    for (int i = 0; i < 20; ++i)
    {
        assert(wrap.update(true, t) == Gesture::None);
        t += 5;
    }
    int count = 0;
    for (int i = 0; i < 100; ++i)
    {
        if (wrap.update(false, t) == Gesture::Single)
        {
            ++count;
        }
        t += 5;
    }
    assert(count == 1);
}

int main()
{
    using actuator::homeAssistantCommand;
    for (int channel = 1; channel <= 6; ++channel)
    {
        char topic[20];
        snprintf(topic, sizeof(topic), "relay/%d/set", channel);
        assert(homeAssistantCommand(topic, "ON", false) == (channel - 1) * 2 + 1);
        assert(homeAssistantCommand(topic, "OFF", false) == (channel - 1) * 2);
        assert(homeAssistantCommand(topic, "ON", true) == -1);
    }
    assert(homeAssistantCommand("identify/set", "PRESS", false) == 12);
    assert(homeAssistantCommand("identify/set", "PRESS", true) == -1);
    for (auto topic : {"", "relay/0/set", "relay/7/set", "relay/11/set", "relay/1/set/extra", "relay/1"})
        assert(homeAssistantCommand(topic, "ON", false) == -1);
    for (auto payload : {"", "on", "ON ", "TOGGLE", "1", "{\"state\":\"ON\"}"})
        assert(homeAssistantCommand("relay/1/set", payload, false) == -1);
    assert(homeAssistantCommand(nullptr, "ON", false) == -1);
    assert(homeAssistantCommand("relay/1/set", nullptr, false) == -1);
    gestureTests();
    protocolTests();
    puts("PASS: gesture sequences, debounce, hold/reset, wraparound, Modbus functions, exceptions, "
         "broadcasts and truncated frames");
}
