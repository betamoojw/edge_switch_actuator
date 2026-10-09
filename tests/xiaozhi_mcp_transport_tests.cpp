#include <XiaozhiMcpTransport.h>
#include <cassert>
#include <iostream>
// Linker fixture: production references the embedded certificate bundle symbols.
asm(".global _binary_src_certs_x509_crt_bundle_start\n_binary_src_certs_x509_crt_bundle_start:\n.byte 1,2,3,4\n.global "
    "_binary_src_certs_x509_crt_bundle_bin_end\n_binary_src_certs_x509_crt_bundle_bin_end:\n");
// Windows/ELF assembly labels match the explicit asm names in the production file.
asm(".global _binary_src_certs_x509_crt_bundle_bin_start\n.set _binary_src_certs_x509_crt_bundle_bin_start, "
    "_binary_src_certs_x509_crt_bundle_start\n");

int main()
{
    using namespace xiaozhi;
    Transport transport;
    unsigned failures = 0, texts = 0, unauthorized = 0;
    std::string received;
    auto callback = [&](Transport::Event event, const char *p, size_t n)
    {
        if (event == Transport::Event::Failed)
        {
            ++failures;
        }
        if (event == Transport::Event::Unauthorized)
        {
            ++unauthorized;
        }
        if (event == Transport::Event::Text)
        {
            ++texts;
            received.assign(p, n);
        }
    };
    transport.open("wss://example.invalid:8443/mcp/?token=test", callback);
    assert(WebSocketsClient::bundle && WebSocketsClient::bundleSize == 4);
    assert(WebSocketsClient::host == "example.invalid" && WebSocketsClient::path == "/mcp/?token=test");
    WebSocketsClient::instance->emit(WStype_TEXT, std::string("ab\0cd", 5));
    assert(received.size() == 5);
    WebSocketsClient::instance->emit(WStype_FRAGMENT_TEXT_START, "{\"a\":");
    WebSocketsClient::instance->emit(WStype_FRAGMENT, "1");
    WebSocketsClient::instance->emit(WStype_FRAGMENT_FIN, "}");
    assert(received == "{\"a\":1}");
    auto count = texts;
    WebSocketsClient::instance->emit(WStype_TEXT, std::string(8193, 'x'));
    assert(failures == 1 && texts == count);
    transport.close();
    transport.open("wss://example.invalid/", callback);
    WebSocketsClient::instance->emit(WStype_FRAGMENT_FIN, "}");
    assert(failures == 2);
    WebSocketsClient::instance->emit(WStype_DISCONNECTED, "HTTP 401");
    assert(unauthorized == 1);
    WebSocketsClient::failConnect = true;
    transport.loop();
    assert(failures == 3);
    transport.close();
    assert(!WebSocketsClient::instance);
    const auto constructions = WebSocketsClient::constructions;
    transport.open("ws://example.invalid/", callback);
    assert(failures == 4 && WebSocketsClient::constructions == constructions);
    WebSocketsClient::failConnect = false;
    FakeSecure::fail = true;
    transport.open("wss://example.invalid/", callback);
    const auto attempts = FakeSecure::connections;
    transport.loop();
    assert(failures == 5 && FakeSecure::connections == attempts);
    transport.loop();
    assert(FakeSecure::connections == attempts); // No automatic retries, even on failure.
    std::cout << "MCP transport framing, verified-TLS configuration and failure signaling passed\n";
}
