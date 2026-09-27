#pragma once
#include <Arduino.h>
#include <esp_system.h>
#include <mbedtls/pkcs5.h>

namespace Password
{
inline String hex(const uint8_t *p, size_t n)
{
    String s;
    s.reserve(n * 2);
    const char *h = "0123456789abcdef";
    while (n--)
    {
        s += h[*p >> 4];
        s += h[*p++ & 15];
    }
    return s;
}

inline bool decode(const String &text, uint8_t *p, size_t n)
{
    if (text.length() != n * 2)
    {
        return false;
    }
    for (size_t i = 0; i < n; ++i)
    {
        char s[3] = {text[i * 2], text[i * 2 + 1], 0};
        char *end;
        p[i] = strtoul(s, &end, 16);
        if (*end)
        {
            return false;
        }
    }
    return true;
}

inline String hash(const String &password)
{
    uint8_t salt[16], key[32];
    esp_fill_random(salt, sizeof(salt));
    if (mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA256, reinterpret_cast<const unsigned char *>(password.c_str()), password.length(), salt,
                                      sizeof(salt), 10000, sizeof(key), key))
    {
        return "";
    }
    return String("pbkdf2$") + hex(salt, 16) + "$" + hex(key, 32);
}

inline bool verify(const String &password, const String &stored)
{
    if (!stored.startsWith("pbkdf2$"))
    {
        return password == stored; // One-time legacy migration occurs on settings load.
    }
    uint8_t salt[16], expected[32], actual[32];
    if (!decode(stored.substring(7, 39), salt, 16) || !decode(stored.substring(40), expected, 32))
    {
        return false;
    }
    if (mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA256, reinterpret_cast<const unsigned char *>(password.c_str()), password.length(), salt,
                                      16, 10000, 32, actual))
    {
        return false;
    }
    uint8_t difference = 0;
    for (int i = 0; i < 32; ++i)
    {
        difference |= actual[i] ^ expected[i];
    }
    return difference == 0;
}
} // namespace Password
