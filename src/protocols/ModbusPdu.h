#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace actuator::wire
{
inline uint16_t word(const uint8_t *p)
{
    return uint16_t(p[0]) << 8 | p[1];
}

inline void put(uint8_t *p, uint16_t v)
{
    p[0] = v >> 8;
    p[1] = v;
}

template <class Backend, class Peer>
size_t processPdu(Backend &backend, const uint8_t *in, size_t n, uint8_t *out, bool serial, const Peer &peer, bool broadcast = false)
{
    if (!n)
    {
        return 0;
    }
    uint8_t fc = in[0];
    auto exception = [&](uint8_t e) -> size_t
    {
        backend.reject();
        out[0] = fc | 0x80;
        out[1] = e;
        return broadcast ? 0 : 2;
    };
    if (broadcast && (fc != 5 && fc != 15))
    {
        return 0;
    }
    backend.accept();
    out[0] = fc;
    if (fc >= 1 && fc <= 4)
    {
        if (n != 5)
        {
            return exception(3);
        }
        uint16_t start = word(in + 1), count = word(in + 3);
        bool bits = fc <= 2;
        if (!count || count > (bits ? 2000 : 125) || uint32_t(start) + count > 65536)
        {
            return exception(3);
        }
        out[1] = bits ? (count + 7) / 8 : count * 2;
        memset(out + 2, 0, out[1]);
        for (uint16_t i = 0; i < count; ++i)
        {
            if (bits)
            {
                bool value;
                if (!backend.readBit(fc == 2, start + i, value, peer))
                {
                    return exception(2);
                }
                if (value)
                {
                    out[2 + i / 8] |= 1 << (i % 8);
                }
            }
            else
            {
                uint16_t v;
                if (!backend.readReg(fc == 4, start + i, v, peer))
                {
                    return exception(2);
                }
                put(out + 2 + i * 2, v);
            }
        }
        return out[1] + 2;
    }
    if (fc == 5 || fc == 6)
    {
        if (n != 5)
        {
            return exception(3);
        }
        uint16_t a = word(in + 1), v = word(in + 3);
        uint8_t error;
        if (broadcast && a >= 6)
        {
            return 0;
        }
        if (fc == 5)
        {
            if (v != 0 && v != 0xff00)
            {
                return exception(3);
            }
            uint8_t bit = v ? 1 : 0;
            error = backend.writeBits(a, 1, &bit, peer);
        }
        else
        {
            error = backend.writeRegs(a, 1, in + 3, peer);
        }
        if (error)
        {
            return exception(error);
        }
        memcpy(out, in, 5);
        return broadcast ? 0 : 5;
    }
    if (fc == 15 || fc == 16)
    {
        if (n < 6)
        {
            return exception(3);
        }
        uint16_t start = word(in + 1), count = word(in + 3);
        size_t size = fc == 15 ? (count + 7) / 8 : count * 2;
        if (!count || count > (fc == 15 ? 1968 : 123) || uint32_t(start) + count > 65536 || in[5] != size || n != 6 + size)
        {
            return exception(3);
        }
        if (broadcast && uint32_t(start) + count > 6)
        {
            return 0;
        }
        uint8_t error = fc == 15 ? backend.writeBits(start, count, in + 6, peer) : backend.writeRegs(start, count, in + 6, peer);
        if (error)
        {
            return exception(error);
        }
        memcpy(out, in, 5);
        return broadcast ? 0 : 5;
    }
    if (fc == 8)
    {
        if (!serial)
        {
            return exception(1);
        }
        if (n != 5)
        {
            return exception(3);
        }
        if (word(in + 1) != 0)
        {
            return exception(1);
        }
        memcpy(out, in, 5);
        return 5;
    }
    if (fc == 43)
    {
        if (n != 4 || in[1] != 14 || in[2] != 1)
        {
            return exception(3);
        }
        if (in[3] > 2)
        {
            return exception(2);
        }
        out[1] = 14;
        out[2] = 1;
        out[3] = 1;
        out[4] = 0;
        out[5] = 0;
        out[6] = 3 - in[3];
        size_t pos = 7;
        const char *identity[] = {"Edge Actuator", "EDGE-S3-6CH", backend.version()};
        for (uint8_t i = in[3]; i < 3; ++i)
        {
            size_t size = strlen(identity[i]);
            out[pos++] = i;
            out[pos++] = size;
            memcpy(out + pos, identity[i], size);
            pos += size;
        }
        return pos;
    }
    return exception(1);
}
} // namespace actuator::wire
