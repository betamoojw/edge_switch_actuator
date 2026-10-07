#include "Modbus.h"
#include "KnxAdapter.h"
#include "ModbusPdu.h"
#include <NetworkSupport.h>
#include <WiFi.h>
#include <algorithm>

namespace actuator
{
static uint16_t word(const uint8_t *p)
{
    return uint16_t(p[0]) << 8 | p[1];
}

static void put(uint8_t *p, uint16_t v)
{
    p[0] = v >> 8;
    p[1] = v;
}

static uint16_t crc(const uint8_t *p, size_t n)
{
    uint16_t c = 0xffff;
    while (n--)
    {
        c ^= *p++;
        for (int i = 0; i < 8; ++i)
        {
            c = (c >> 1) ^ ((c & 1) ? 0xa001 : 0);
        }
    }
    return c;
}

bool Modbus::begin()
{
    if (d.config.mode == Rtu)
    {
        static const uint32_t speeds[] = {9600, 19200, 38400, 57600, 115200};
        static const uint32_t formats[] = {SERIAL_8E1, SERIAL_8O1, SERIAL_8N2, SERIAL_8N1};
        uint32_t baud = speeds[d.config.baud];
        gap = baud > 19200 ? 1750 : (38500000u + baud - 1) / baud;
        if (!frames)
        {
            frames = xQueueCreate(4, sizeof(RtuFrame));
        }
        if (!frames)
        {
            return false;
        }
        uart.setRxBufferSize(1024);
        uart.begin(baud, formats[d.config.serialFormat], board::Rs485RxPin, board::Rs485TxPin);
        // Let UART hardware detect silence; task scheduling cannot timestamp bytes.
        uart.setRxTimeout(uint8_t((uint64_t(baud) * gap + 10999999) / 11000000));
        uart.onReceive(
            [this]()
            {
                RtuFrame frame;
                while (uart.available())
                {
                    int b = uart.read();
                    if (frame.size < sizeof(frame.bytes))
                    {
                        frame.bytes[frame.size++] = b;
                    }
                    else
                    {
                        frame.overflow = true;
                    }
                }
                if (frame.size)
                {
                    xQueueSend(frames, &frame, 0);
                }
            },
            true);
        return bool(uart);
    }
    server.reset(new NetworkServer(NetworkSupport::localIP(), d.config.tcpPort));
    server->begin();
    return bool(*server);
}

void Modbus::stop()
{
    uart.onReceive(nullptr);
    uart.end();
    if (frames)
    {
        xQueueReset(frames);
    }
    if (server)
    {
        server->end();
        server.reset();
    }
    for (auto &p : peers)
    {
        p.socket.stop();
        p.size = 0;
    }
    windowUntil = 0;
    dirty = false;
}

void Modbus::window(uint32_t seconds, const String &peer)
{
    windowPeer = peer;
    windowUntil = seconds ? millis() + seconds * 1000 : 0;
    staged = d.config;
    dirty = false;
}

bool Modbus::allowed(const String &peer) const
{
    return windowOpen() && windowPeer == peer;
}

uint8_t Modbus::clients()
{
    uint8_t n = 0;
    for (auto &p : peers)
    {
        if (p.socket)
        {
            ++n;
        }
    }
    return n;
}

void Modbus::loop()
{
    if (windowUntil && !windowOpen())
    {
        windowUntil = 0;
        dirty = false;
    }
    if (d.config.mode == Rtu)
    {
        RtuFrame frame;
        if (frames && xQueueReceive(frames, &frame, 0) == pdTRUE)
        {
            auto *rtu = frame.bytes;
            size_t rtuSize = frame.size;
            if (!frame.overflow && rtuSize >= 4 && crc(rtu, rtuSize) == 0)
            {
                bool broadcast = rtu[0] == 0;
                if (rtu[0] == d.config.unit || broadcast)
                {
                    uint8_t out[256];
                    out[0] = rtu[0];
                    size_t n = pdu(rtu + 1, rtuSize - 3, out + 1, true, "rtu", broadcast);
                    if (!broadcast && n)
                    {
                        uint16_t c = crc(out, n + 1);
                        out[n + 1] = c;
                        out[n + 2] = c >> 8;
                        uart.write(out, n + 3);
                        uart.flush();
                    }
                }
            }
            else
            {
                ++errors;
            }
        }
        return;
    }
    if (!server)
    {
        return;
    }
    auto incoming = server->accept();
    if (incoming)
    {
        bool assigned = false;
        String ip = incoming.remoteIP().toString();
        if (d.config.tcpAllow.isEmpty() || d.config.tcpAllow == ip)
        {
            for (auto &p : peers)
            {
                if (!p.socket.connected())
                {
                    p.socket = incoming;
                    p.size = 0;
                    p.last = millis();
                    p.session = ip + ":" + String(++sessionCounter);
                    assigned = true;
                    break;
                }
            }
        }
        if (!assigned)
        {
            incoming.stop();
        }
    }
    for (auto &p : peers)
    {
        if (!p.socket.connected())
        {
            p.size = 0;
            continue;
        }
        if (uint32_t(millis() - p.last) > 60000)
        {
            p.socket.stop();
            p.size = 0;
            continue;
        }
        size_t budget = 520;
        while (p.socket.available() && budget--)
        {
            if (p.size == sizeof(p.bytes))
            {
                p.socket.stop();
                p.size = 0;
                ++errors;
                break;
            }
            p.bytes[p.size++] = p.socket.read();
            p.last = millis();
            if (p.size < 7)
            {
                continue;
            }
            uint16_t length = word(p.bytes + 4);
            if (word(p.bytes + 2) != 0 || length < 2 || length > 254)
            {
                p.socket.stop();
                p.size = 0;
                ++errors;
                break;
            }
            if (p.size < size_t(6 + length))
            {
                continue;
            }
            uint8_t reply[260];
            memcpy(reply, p.bytes, 7);
            size_t n;
            if (p.bytes[6] != d.config.unit)
            {
                reply[7] = p.bytes[7] | 0x80;
                reply[8] = 0x0b;
                n = 2;
            }
            else
            {
                if (windowOpen() && windowPeer == p.socket.remoteIP().toString())
                {
                    windowPeer = p.session;
                }
                n = pdu(p.bytes + 7, length - 1, reply + 7, false, p.session);
            }
            put(reply + 4, n + 1);
            if (n)
            {
                p.socket.write(reply, n + 7);
            }
            p.size = 0;
        }
    }
}

bool Modbus::readBit(bool di, uint16_t a, bool &v, const String &peer)
{
    auto &c = allowed(peer) ? staged : d.config;
    if (a < 6)
    {
        v = d.outputs[a];
        return true;
    }
    if (!di)
    {
        if (a >= 0x10 && a <= 0x15)
        {
            v = c.relays[a - 0x10].enabled;
            return true;
        }
        switch (a)
        {
            case 0x20:
                v = c.rgb;
                break;
            case 0x21:
                v = c.buzzer;
                break;
            case 0x22:
                v = c.button;
                break;
            case 0x30:
                v = d.manualRgb;
                break;
            case 0x31:
                v = d.toneNow != 0;
                break;
            default:
                return false;
        }
        return true;
    }
    if (a >= 0x30 && a <= 0x35)
    {
        v = d.blocks[a - 0x30];
        return true;
    }
    switch (a)
    {
        case 0x10:
            v = d.gesture.pressed();
            break;
        case 0x11:
            v = d.config.button;
            break;
        case 0x12:
            v = d.rgbNow[0] || d.rgbNow[1] || d.rgbNow[2];
            break;
        case 0x13:
            v = d.toneNow;
            break;
        case 0x14:
            v = NetworkSupport::online();
            break;
        case 0x15:
            v = WiFi.getMode() & WIFI_MODE_AP;
            break;
        case 0x16:
            v = d.fault;
            break;
        case 0x17:
            v = false;
            break;
        case 0x20:
            v = d.config.rgb;
            break;
        case 0x21:
            v = d.config.buzzer;
            break;
        case 0x22:
            v = true;
            break;
        case 0x23:
            v = d.config.rs485;
            break;
        case 0x24:
            v = d.config.mode == Rtu && d.protocolState == "running";
            break;
        case 0x25:
            v = d.protocolState == "running";
            break;
        case 0x26:
            v = windowOpen();
            break;
        default:
            return false;
    }
    return true;
}

bool Modbus::readReg(bool input, uint16_t a, uint16_t &v, const String &peer)
{
    auto &c = allowed(peer) ? staged : d.config;
    if (!input)
    {
        if (a >= 0x100 && a <= 0x104)
        {
            v = manual[a - 0x100];
            return true;
        }
        if (a >= 0x110 && a <= 0x112)
        {
            v = manual[a - 0x110 + 5];
            return true;
        }
        if (a >= 0x120 && a <= 0x125)
        {
            auto b = c.clicks[(a - 0x120) / 2];
            v = (a & 1) ? b.target : b.action;
            return true;
        }
        for (int base : {0x130, 0x140, 0x150, 0x160})
        {
            if (a >= base && a < base + 6)
            {
                auto r = c.relays[a - base];
                v = base == 0x130 ? r.startup : base == 0x140 ? r.pulseMs : base == 0x150 ? r.disconnectOff : r.timeout;
                return true;
            }
        }
        if (a >= 0x200 && a <= 0x203)
        {
            v = 0;
            return true;
        }
        return false;
    }
    auto counter = [&](uint16_t start, uint32_t n) { v = a == start ? n >> 16 : n; };
    if (a >= 6 && a <= 7)
    {
        counter(6, d.gestureCount);
        return true;
    }
    if (a >= 8 && a <= 9)
    {
        counter(8, sampledUptime);
        return true;
    }
    if (a >= 10 && a <= 11)
    {
        counter(10, d.config.revision);
        return true;
    }
    if (a >= 0x10 && a <= 0x15)
    {
        counter(a & 0xfffe, a <= 0x11 ? requests : a <= 0x13 ? errors : rejected);
        return true;
    }
    switch (a)
    {
        case 0:
            v = 1;
            break;
        case 1:
            v = 255;
            break;
        case 2:
            v = d.config.mode;
            break;
        case 3:
            v = NetworkSupport::online() ? (WiFi.getMode() & WIFI_MODE_AP ? 4 : 3) : (WiFi.getMode() & WIFI_MODE_AP ? 1 : 2);
            break;
        case 4:
            v = d.fault;
            break;
        case 5:
            v = d.lastGesture;
            break;
        case 0x20:
            v = d.config.unit;
            break;
        case 0x21:
            v = d.config.baud;
            break;
        case 0x22:
            v = d.config.serialFormat;
            break;
        case 0x23:
            v = d.config.tcpPort;
            break;
        case 0x24:
            v = clients();
            break;
        case 0x25:
            v = lastSequence;
            break;
        case 0x26:
            v = lastResult;
            break;
        case 0x27:
            v = lastDetail;
            break;
        case 0x30:
        case 0x31:
        case 0x32:
            v = d.rgbNow[a - 0x30];
            break;
        case 0x33:
            v = d.toneNow;
            break;
        case 0x34:
            v = d.gesture.armed(millis()) ? 6 : d.fault ? 3 : d.knx->isProgramming() ? 5 : d.manualRgb ? 2 : 1;
            break;
        default:
            return false;
    }
    return true;
}

uint8_t Modbus::writeBits(uint16_t start, uint16_t count, const uint8_t *bits, const String &peer)
{
    Config next = staged;
    bool configWrite = false;
    for (uint32_t i = 0; i < count; ++i)
    {
        uint32_t a = start + i;
        bool v = bits[i / 8] & (1 << (i % 8));
        if (a < 6)
        {
            if (!(d.config.busMask & (1 << a)) || !d.config.relays[a].enabled || d.blocks[a])
            {
                return 2;
            }
        }
        else if (a >= 0x10 && a <= 0x15)
        {
            if (!allowed(peer))
            {
                return 2;
            }
            next.relays[a - 0x10].enabled = v;
            configWrite = true;
        }
        else if (a >= 0x20 && a <= 0x22)
        {
            if (!allowed(peer))
            {
                return 2;
            }
            if (a == 0x20)
            {
                next.rgb = v;
            }
            if (a == 0x21)
            {
                next.buzzer = v;
            }
            if (a == 0x22)
            {
                next.button = v;
            }
            configWrite = true;
        }
        else if (a == 0x30 || a == 0x31)
        {
            if (!d.config.busIndicators || (a == 0x30 && !d.config.rgb) || (a == 0x31 && !d.config.buzzer))
            {
                return 2;
            }
        }
        else
        {
            return 2;
        }
    }
    for (uint32_t i = 0; i < count; ++i)
    {
        uint32_t a = start + i;
        bool v = bits[i / 8] & (1 << (i % 8));
        if (a < 6)
        {
            d.relay(a, v, "modbus");
        }
        if (a == 0x30)
        {
            if (v && !d.manualRgb)
            {
                for (int c = 0; c < 3; ++c)
                {
                    d.manualColor[c] = manual[c];
                }
                d.manualBrightness = manual[3];
                d.rgbUntil = millis() + manual[4] * 1000;
                d.manualRgb = true;
            }
            else if (!v)
            {
                d.manualRgb = false;
            }
        }
        if (a == 0x31)
        {
            if (v && !d.toneNow)
            {
                d.tone(manual[5], manual[6], manual[7]);
            }
            else if (!v)
            {
                d.toneNow = 0;
                d.toneUntil = 0;
            }
        }
    }
    if (configWrite)
    {
        staged = next;
        dirty = true;
    }
    return 0;
}

uint8_t Modbus::writeRegs(uint16_t start, uint16_t count, const uint8_t *bytes, const String &peer)
{
    if (start == 0x200 && count == 4)
    {
        auto op = word(bytes), target = word(bytes + 2), seq = word(bytes + 4), trigger = word(bytes + 6);
        if (trigger == 0)
        {
            return 0;
        }
        if (trigger != 0xa55a || !seq || op < 1 || op > 5)
        {
            return 3;
        }
        for (auto &h : history)
        {
            if (h.seq == seq && h.peer == peer && uint32_t(millis() - h.at) < 60000)
            {
                if (h.op != op || h.target != target)
                {
                    return 3;
                }
                lastSequence = seq;
                lastResult = h.result;
                lastDetail = h.detail;
                return 0;
            }
        }
        if ((op >= 4 && !allowed(peer)) || (op <= 2 && !d.config.busIndicators))
        {
            return 2;
        }
        if (op == 3 && (target > 6 || (target && !(d.config.busMask & (1 << (target - 1))))))
        {
            return 2;
        }
        if (op == 3 && target == 0)
        {
            for (uint8_t c = 0; c < 6; ++c)
            {
                if (d.config.relays[c].enabled && !(d.config.busMask & (1 << c)))
                {
                    return 2;
                }
            }
        }
        if (op != 3 && target)
        {
            return 3;
        }
        lastSequence = seq;
        lastResult = 2;
        lastDetail = 0;
        if (op == 1)
        {
            d.identify();
        }
        if (op == 2)
        {
            d.toneNow = 0;
            d.toneUntil = 0;
        }
        if (op == 3)
        {
            // Preflight the entire selection before pulsing any output.
            for (uint8_t c = 0; c < 6; ++c)
            {
                if ((target == c + 1 || (!target && d.config.relays[c].enabled)) &&
                    (!d.config.relays[c].enabled || d.blocks[c]))
                {
                    lastResult = 3;
                    lastDetail = d.blocks[c] ? 3 : 2;
                }
            }
            if (lastResult == 2)
            {
                for (uint8_t c = 0; c < 6; ++c)
                {
                    if (target == c + 1 || (!target && d.config.relays[c].enabled))
                    {
                        d.relay(c, true, "modbus", true);
                    }
                }
            }
        }
        if (op == 4)
        {
            String e;
            JsonDocument doc;
            staged.write(doc.to<JsonObject>());
            Config checked;
            if (!Config::parse(doc.as<JsonObjectConst>(), checked, e))
            {
                lastResult = 3;
                lastDetail = 1;
            }
            else if (staged.revision != d.config.revision)
            {
                lastResult = 3;
                lastDetail = 6;
            }
            else
            { // These staged fields do not change the transport. Commit without closing the reply
              // socket.
                checked.revision++;
                doc.clear();
                checked.write(doc.to<JsonObject>());
                String data;
                serializeJson(doc, data);
                if (!d.store.write("/config/actuator", data))
                {
                    lastResult = 4;
                    lastDetail = 5;
                }
                else
                {
                    d.config = checked;
                    d.disableConfiguredRelays();
                    staged = d.config;
                    dirty = false;
                }
            }
        }
        if (op == 5)
        {
            staged = d.config;
            dirty = false;
        }
        history[historyHead++ % history.size()] = {peer, seq, op, target, lastResult, lastDetail, millis()};
        return 0;
    }
    Config next = staged;
    uint16_t values[8];
    memcpy(values, manual, sizeof(values));
    bool changed = false;
    for (uint32_t i = 0; i < count; ++i)
    {
        uint32_t a = start + i;
        uint16_t v = word(bytes + i * 2);
        if (a >= 0x100 && a <= 0x104)
        {
            if (!d.config.busIndicators)
            {
                return 2;
            }
            int x = a - 0x100;
            if (v > (x < 3 ? 255 : x == 3 ? 100 : 30) || (x == 4 && !v))
            {
                return 3;
            }
            values[x] = v;
        }
        else if (a >= 0x110 && a <= 0x112)
        {
            if (!d.config.busIndicators)
            {
                return 2;
            }
            int x = a - 0x110;
            if ((x == 0 && (v < 500 || v > 4000)) || (x == 1 && (v < 10 || v > 2000)) || (x == 2 && (v < 1 || v > 50)))
            {
                return 3;
            }
            values[x + 5] = v;
        }
        else
        {
            if (!allowed(peer))
            {
                return 2;
            }
            changed = true;
            if (a >= 0x120 && a <= 0x125)
            {
                if (v > ((a & 1) ? 6 : 8))
                {
                    return 3;
                }
                auto &b = next.clicks[(a - 0x120) / 2];
                if (a & 1)
                {
                    b.target = v;
                }
                else
                {
                    b.action = v;
                }
            }
            else if (a >= 0x130 && a <= 0x135)
            {
                if (v > 1)
                {
                    return 3;
                }
                next.relays[a - 0x130].startup = v;
            }
            else if (a >= 0x140 && a <= 0x145)
            {
                if (v < 10 || v > 60000)
                {
                    return 3;
                }
                next.relays[a - 0x140].pulseMs = v;
            }
            else if (a >= 0x150 && a <= 0x155)
            {
                if (v > 1)
                {
                    return 3;
                }
                next.relays[a - 0x150].disconnectOff = v;
            }
            else if (a >= 0x160 && a <= 0x165)
            {
                if (v < 1 || v > 3600)
                {
                    return 3;
                }
                next.relays[a - 0x160].timeout = v;
            }
            else
            {
                return 2;
            }
        }
    }
    memcpy(manual, values, sizeof(manual));
    if (changed)
    {
        staged = next;
        dirty = true;
    }
    return 0;
}

size_t Modbus::pdu(const uint8_t *in, size_t n, uint8_t *out, bool serial, const String &peer, bool broadcast)
{
    return wire::processPdu(*this, in, n, out, serial, peer, broadcast);
}
} // namespace actuator
