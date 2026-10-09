#pragma once
#include <Features.h>
#if FT_ENABLED(FT_XIAOZHI_MCP)
#include "XiaozhiMcpProtocol.h"
#include <WebSocketsClient.h>
#include <memory>

namespace xiaozhi
{
class Transport
{
  public:
    enum class Event
    {
        Connected,
        Text,
        Failed,
        Unauthorized
    };
    using Callback = std::function<void(Event, const char *, size_t)>;
    void open(const std::string &endpoint, Callback callback);
    void close();
    void loop();

    bool send(const std::string &text)
    {
        return socket && socket->sendTXT(reinterpret_cast<const uint8_t *>(text.data()), text.size());
    }

  private:
    class Client: public WebSocketsClient
    {
        bool failedConnect = false;

      public:
        // 2.7.2 does not expose the TLS handshake timeout. Establish its secure
        // client here, then let the pinned library own framing and cleanup.
        void connectVerified()
        {
            _client.ssl = new WEBSOCKETS_NETWORK_SSL_CLASS();
            _client.tcp = _client.ssl;
            _client.ssl->setCACertBundle(_CA_bundle, _CA_bundle_size);
            _client.ssl->setHandshakeTimeout(5);
            failedConnect = !_client.ssl->connect(_host.c_str(), _port, 5000);
            if (!failedConnect)
            {
                connectedCb();
            }
        }

        void loop()
        {
            if (failedConnect)
            {
                return;
            }
            // Always suppress the library's automatic connection path, including
            // a TCP drop between state checks. The service creates each attempt.
            _lastConnectionFail = millis();
            WebSocketsClient::loop();
        }

        bool attemptFailed() const
        {
            return failedConnect;
        }
    };

    std::unique_ptr<Client> socket;
    Callback callback;
    std::string fragment;
    bool fragmented = false;
    void event(WStype_t type, uint8_t *payload, size_t length);
};
} // namespace xiaozhi
#endif
