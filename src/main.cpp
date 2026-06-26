#include <iostream>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include "ws_client.hpp"

namespace net = boost::asio;
namespace ssl = boost::asio::ssl;

int main() {
    std::cout << "[order-sentinel] starting up...\n";
    std::cout << "  target : wss://stream.binance.com:9443\n";
    std::cout << "  stream : btcusdt@depth\n";

    // io_context is the event loop. Every async operation is registered here.
    // ioc.run() blocks the current thread and processes events until there is
    // nothing left to do (or we call ioc.stop()).
    net::io_context ioc;

    // ssl::context holds TLS configuration: which protocols to allow,
    // certificate authorities to trust, etc.
    // tlsv12_client = TLS 1.2+ as a client (not a server)
    ssl::context ctx(ssl::context::tlsv12_client);

    // Trust the OS's built-in certificate authorities (same ones your browser uses).
    // Without this, the TLS handshake would fail because we can't verify Binance's cert.
    ctx.set_default_verify_paths();

    // Create the client. We use make_shared because WsClient inherits from
    // enable_shared_from_this — it must always live inside a shared_ptr.
    auto client = std::make_shared<WsClient>(ioc, ctx,
        [](const std::string& msg) {
            // Week 3 will parse this JSON into an order book update.
            // For now, just print the first 120 characters so we can see it's working.
            std::cout << "[msg] " << msg.substr(0, 120) << "...\n";
        }
    );

    // Queue up the async connection chain (returns immediately)
    client->connect("stream.binance.com", "9443", "/ws/btcusdt@depth");

    // Actually run the event loop — this is where all the async work happens.
    // Blocks until the WebSocket closes or an unrecovered error occurs.
    ioc.run();

    return 0;
}