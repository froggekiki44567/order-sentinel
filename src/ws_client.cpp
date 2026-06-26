#include "ws_client.hpp"
#include <chrono>
#include <iostream>

// --- Constructor ---
// net::make_strand(ioc) creates a "strand" — a serialization wrapper that
// guarantees callbacks don't run concurrently even if ioc uses multiple threads.
// For now we have one thread, but it's the correct habit for production async code.
WsClient::WsClient(net::io_context& ioc,
                   ssl::context&    ctx,
                   MessageCallback  on_message)
    : resolver_(net::make_strand(ioc))
    , ws_(net::make_strand(ioc), ctx)
    , on_message_(std::move(on_message))
{}

// --- Step 0: kick off the chain ---
void WsClient::connect(const std::string& host,
                       const std::string& port,
                       const std::string& path)
{
    host_ = host;
    path_ = path;

    // async_resolve does a DNS lookup without blocking.
    // bind_front_handler is Beast's shorthand for std::bind + shared_from_this:
    // it creates a callable that, when invoked, calls this->on_resolve(ec, results).
    resolver_.async_resolve(
        host, port,
        beast::bind_front_handler(&WsClient::on_resolve, shared_from_this())
    );
}

// --- Step 1: DNS resolved → open TCP connection ---
void WsClient::on_resolve(beast::error_code ec,
                           tcp::resolver::results_type results)
{
    if (ec) {
        std::cerr << "[ws] resolve error: " << ec.message() << "\n";
        return;
    }

    // Set a 30-second deadline for the TCP connect attempt.
    // expires_after() works on the lowest layer (the raw tcp_stream).
    beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));

    beast::get_lowest_layer(ws_).async_connect(
        results,
        beast::bind_front_handler(&WsClient::on_connect, shared_from_this())
    );
}

// --- Step 2: TCP connected → TLS handshake ---
void WsClient::on_connect(beast::error_code ec,
                           tcp::resolver::results_type::endpoint_type ep)
{
    if (ec) {
        std::cerr << "[ws] connect error: " << ec.message() << "\n";
        return;
    }

    // Append the port to host_ so the HTTP Host header reads "stream.binance.com:9443".
    // This is required by the WebSocket handshake spec and by SNI (TLS extension
    // that tells the server which certificate to present when multiple sites share an IP).
    host_ += ":" + std::to_string(ep.port());

    beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));

    // ws_.next_layer() peels off the WebSocket layer → gives us the ssl_stream.
    // We tell it we're a client (not a server).
    ws_.next_layer().async_handshake(
        ssl::stream_base::client,
        beast::bind_front_handler(&WsClient::on_ssl_handshake, shared_from_this())
    );
}

// --- Step 3: TLS done → WebSocket upgrade ---
void WsClient::on_ssl_handshake(beast::error_code ec)
{
    if (ec) {
        std::cerr << "[ws] SSL handshake error: " << ec.message() << "\n";
        return;
    }

    // Now that we're in WebSocket mode, disable the TCP-level timeout.
    // WebSocket has its own ping/pong keep-alive; Beast handles that via
    // the "suggested" timeout settings below.
    beast::get_lowest_layer(ws_).expires_never();

    // Apply Beast's recommended WebSocket timeouts (idle ping, response deadline, etc.)
    ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));

    // Set a User-Agent header on the HTTP upgrade request.
    // Some servers reject connections without one.
    ws_.set_option(websocket::stream_base::decorator(
        [](websocket::request_type& req) {
            req.set(boost::beast::http::field::user_agent, "order-sentinel/0.1");
        }
    ));

    // Send the HTTP GET upgrade request that switches the protocol to WebSocket.
    // host_ = "stream.binance.com:9443", path_ = "/ws/btcusdt@depth"
    ws_.async_handshake(
        host_, path_,
        beast::bind_front_handler(&WsClient::on_handshake, shared_from_this())
    );
}

// --- Step 4: WebSocket handshake done → start reading ---
void WsClient::on_handshake(beast::error_code ec)
{
    if (ec) {
        std::cerr << "[ws] WebSocket handshake error: " << ec.message() << "\n";
        return;
    }

    std::cout << "[ws] connected to " << host_ << path_ << "\n";
    do_read();
}

// --- Step 5: post a single async read ---
void WsClient::do_read()
{
    // async_read fills buffer_ until one complete WebSocket frame arrives,
    // then calls on_read. A "frame" from Binance is one JSON message.
    ws_.async_read(
        buffer_,
        beast::bind_front_handler(&WsClient::on_read, shared_from_this())
    );
}

// --- Step 5 (repeating): frame received ---
void WsClient::on_read(beast::error_code ec, std::size_t /*bytes_transferred*/)
{
    if (ec) {
        std::cerr << "[ws] read error: " << ec.message() << "\n";
        return; // stops the read loop; Week 5 will add reconnect logic
    }

    // buffers_to_string converts Beast's internal buffer to a plain std::string
    on_message_(beast::buffers_to_string(buffer_.data()));

    // IMPORTANT: clear the buffer before the next read, otherwise frames accumulate
    buffer_.consume(buffer_.size());

    // Go back to step 5 — this creates the infinite read loop
    do_read();
}
