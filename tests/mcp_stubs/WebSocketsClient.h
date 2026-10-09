#pragma once
#include <Arduino.h>
#include <cassert>
#include <cstdint>
#include <functional>
#include <string>

class FakeSecure
{
    bool trusted = false, bounded = false;

  public:
    inline static bool fail = false;
    inline static unsigned connections = 0;

    void setCACertBundle(const uint8_t *bundle, size_t size)
    {
        assert(bundle && size == 4);
        trusted = true;
    }

    void setHandshakeTimeout(unsigned seconds)
    {
        assert(seconds == 5);
        bounded = true;
    }

    bool connect(const char *, uint16_t, int timeout)
    {
        assert(timeout == 5000);
        assert(trusted && bounded);
        ++connections;
        return !fail;
    }
};

#define WEBSOCKETS_NETWORK_SSL_CLASS FakeSecure

enum WStype_t
{
    WStype_CONNECTED,
    WStype_DISCONNECTED,
    WStype_ERROR,
    WStype_BIN,
    WStype_TEXT,
    WStype_FRAGMENT_BIN_START,
    WStype_FRAGMENT_TEXT_START,
    WStype_FRAGMENT,
    WStype_FRAGMENT_FIN
};

class WebSocketsClient
{
  protected:
    unsigned long _lastConnectionFail = 0;
    const uint8_t *_CA_bundle = nullptr;
    size_t _CA_bundle_size = 0;
    std::string _host;
    uint16_t _port = 443;

    struct
    {
        FakeSecure *ssl = nullptr, *tcp = nullptr;
    } _client;

    void connectedCb()
    {
    }

  public:
    inline static WebSocketsClient *instance = nullptr;
    inline static bool failConnect = false;
    inline static unsigned constructions = 0, loops = 0;
    inline static std::string sent, host, path;
    inline static const uint8_t *bundle = nullptr;
    inline static size_t bundleSize = 0;
    std::function<void(WStype_t, uint8_t *, size_t)> callback;

    WebSocketsClient()
    {
        instance = this;
        ++constructions;
    }

    virtual ~WebSocketsClient()
    {
        delete _client.ssl;
        instance = nullptr;
    }

    void onEvent(decltype(callback) cb)
    {
        callback = cb;
    }

    void setReconnectInterval(unsigned long interval)
    {
        assert(interval == UINT32_MAX);
    }

    void enableHeartbeat(uint32_t ping, uint32_t pong, uint8_t count)
    {
        if (ping != 10000 || pong != 10000 || count != 2)
        {
            std::abort();
        }
    }

    void beginSslWithBundle(const char *h, uint16_t, const char *p, const uint8_t *ca, size_t size, const char *)
    {
        host = h;
        path = p;
        bundle = ca;
        bundleSize = size;
        _CA_bundle = ca;
        _CA_bundle_size = size;
        _host = h;
    }

    void disconnect()
    {
    }

    void loop()
    {
        ++loops;
        if (failConnect)
        {
            emit(WStype_DISCONNECTED, "Connection lost");
        }
    }

    bool sendTXT(const uint8_t *p, size_t n)
    {
        sent.assign(reinterpret_cast<const char *>(p), n);
        return true;
    }

    void emit(WStype_t event, const std::string &s)
    {
        callback(event, reinterpret_cast<uint8_t *>(const_cast<char *>(s.data())), s.size());
    }
};
