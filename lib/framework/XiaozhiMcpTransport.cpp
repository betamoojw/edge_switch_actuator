#include "XiaozhiMcpTransport.h"
#if FT_ENABLED(FT_XIAOZHI_MCP)
extern const uint8_t rootca_crt_bundle_start[] asm("_binary_src_certs_x509_crt_bundle_bin_start");
extern const uint8_t rootca_crt_bundle_end[] asm("_binary_src_certs_x509_crt_bundle_bin_end");

namespace xiaozhi
{
void Transport::open(const std::string &url, Callback cb)
{
    close();
    callback = std::move(cb);
    Endpoint endpoint;
    if (!parseEndpoint(url, endpoint))
    {
        callback(Event::Failed, nullptr, 0);
        return;
    }
    socket.reset(new Client());
    socket->onEvent([this](WStype_t type, uint8_t *payload, size_t length) { event(type, payload, length); });
    // Our worker owns every connection attempt; never use library auto-reconnect.
    socket->setReconnectInterval(UINT32_MAX);
    socket->enableHeartbeat(10000, 10000, 2);
    socket->beginSslWithBundle(endpoint.host.c_str(), endpoint.port, endpoint.path.c_str(), rootca_crt_bundle_start,
                               rootca_crt_bundle_end - rootca_crt_bundle_start, "");
    socket->connectVerified();
}

void Transport::close()
{
    callback = nullptr;
    if (socket)
    {
        socket->disconnect();
    }
    socket.reset();
    fragment.clear();
    fragmented = false;
}

void Transport::loop()
{
    if (!socket)
    {
        return;
    }
    socket->loop();
    // Surface a failed verified connect; the worker destroys this attempt.
    if (socket->attemptFailed() && callback)
    {
        callback(Event::Failed, nullptr, 0);
    }
}

void Transport::event(WStype_t type, uint8_t *payload, size_t length)
{
    if (!callback)
    {
        return;
    }
    if (type == WStype_CONNECTED)
    {
        callback(Event::Connected, nullptr, 0);
        return;
    }
    if (type == WStype_DISCONNECTED || type == WStype_ERROR || type == WStype_BIN || type == WStype_FRAGMENT_BIN_START)
    {
        fragment.clear();
        fragmented = false;
        // Inspect only the bounded status marker, never expose a remote reason string.
        const std::string reason = payload && length < 128 ? std::string(reinterpret_cast<char *>(payload), length) : "";
        callback(reason.find("HTTP 401") != std::string::npos || reason.find("HTTP 403") != std::string::npos ? Event::Unauthorized
                                                                                                              : Event::Failed,
                 nullptr, 0);
        return;
    }
    if (type == WStype_TEXT)
    {
        if (length > MaxMessage || fragmented)
        {
            callback(Event::Failed, nullptr, 0);
        }
        else
        {
            callback(Event::Text, reinterpret_cast<char *>(payload), length);
        }
    }
    else if (type == WStype_FRAGMENT_TEXT_START || type == WStype_FRAGMENT || type == WStype_FRAGMENT_FIN)
    {
        if (type == WStype_FRAGMENT_TEXT_START)
        {
            if (fragmented)
            {
                callback(Event::Failed, nullptr, 0);
                return;
            }
            fragment.clear();
            fragmented = true;
        }
        if (!fragmented || length > MaxMessage - fragment.size())
        {
            callback(Event::Failed, nullptr, 0);
            return;
        }
        if (length)
        {
            fragment.append(reinterpret_cast<char *>(payload), length);
        }
        if (type == WStype_FRAGMENT_FIN)
        {
            fragmented = false;
            callback(Event::Text, fragment.data(), fragment.size());
            fragment.clear();
        }
    }
}
} // namespace xiaozhi
#endif
