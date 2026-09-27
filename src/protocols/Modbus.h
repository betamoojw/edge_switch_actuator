#pragma once
#include "device/Actuator.h"
#include <array>

namespace actuator
{
class Modbus
{
public:
    explicit Modbus(Actuator &d) : d(d), uart(1)
    {
    }

    bool begin();
    void stop();
    void loop();
    void window(uint32_t seconds, const String &peer);

    bool windowOpen() const
    {
        return windowUntil && int32_t(windowUntil - millis()) > 0;
    }

    uint32_t requests = 0, errors = 0, rejected = 0, sampledUptime = 0;
    uint8_t clients();
    size_t pdu(const uint8_t *in, size_t len, uint8_t *out, bool serial, const String &peer, bool broadcast = false);

private:
    Actuator &d;
    HardwareSerial uart;
    std::unique_ptr<NetworkServer> server;

    struct Client
    {
        NetworkClient socket;
        uint8_t bytes[260];
        size_t size = 0;
        uint32_t last = 0;
        String session;
    };

    std::array<Client, 4> peers;

    struct RtuFrame
    {
        uint8_t bytes[256];
        uint16_t size = 0;
        bool overflow = false;
    };

    QueueHandle_t frames = nullptr;
    uint32_t gap = 1750, windowUntil = 0, sessionCounter = 0;
    String windowPeer;
    Config staged;
    bool dirty = false;
    uint16_t manual[8] = {0, 0, 0, 10, 5, 2000, 100, 25}, lastSequence = 0, lastResult = 0, lastDetail = 0;

    struct Result
    {
        String peer;
        uint16_t seq = 0, op = 0, target = 0, result = 0, detail = 0;
        uint32_t at = 0;
    };

    std::array<Result, 16> history;
    uint8_t historyHead = 0;
    bool allowed(const String &peer) const;

public: // Adapter operations used by the portable PDU engine.
    void accept()
    {
        ++requests;
        d.lastBus = millis();
        sampledUptime = millis() / 1000;
    }

    void reject()
    {
        ++rejected;
    }

    const char *version() const
    {
        return APP_VERSION;
    }

    bool readBit(bool discrete, uint16_t address, bool &value, const String &peer);
    bool readReg(bool input, uint16_t address, uint16_t &value, const String &peer);
    uint8_t writeBits(uint16_t start, uint16_t count, const uint8_t *bits, const String &peer);
    uint8_t writeRegs(uint16_t start, uint16_t count, const uint8_t *bytes, const String &peer);
};
} // namespace actuator
