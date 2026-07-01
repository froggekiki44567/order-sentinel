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

    net::io_context ioc;

    ssl::context ctx(ssl::context::tlsv12_client);

    ctx.set_default_verify_paths();

    auto client = std::make_shared<WsClient>(ioc, ctx,
        [](const std::string& msg) {
            std::cout << "[msg] " << msg.substr(0, 120) << "...\n";
        }
    );

    // Queue up the async connection chain (returns immediately)
    client->connect("stream.binance.com", "9443", "/ws/btcusdt@depth");

    // Blocks until the WebSocket closes or an unrecovered error occurs.
    ioc.run();

    return 0;
}