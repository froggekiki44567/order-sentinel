#pragma once

// Beast is Boost's networking library. It sits on top of Asio (async I/O engine)
// and adds HTTP + WebSocket protocol support.
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>

#include <functional>
#include <memory>
#include <string>

// Namespace aliases — Beast's real names are long; these are standard shortcuts
namespace beast     = boost::beast;
namespace websocket = beast::websocket;
namespace net       = boost::asio;
namespace ssl       = boost::asio::ssl;
using     tcp       = net::ip::tcp;

// WsClient connects to a secure WebSocket server (wss://) and fires
// on_message for every JSON frame received.
//
// WHY inherit from enable_shared_from_this?
//   Async callbacks run later, after connect() returns. By the time the callback
//   fires, the caller's local variable might be gone. shared_from_this() lets
//   the object keep itself alive by holding a shared_ptr to itself for as long
//   as there is a pending async operation.
class WsClient : public std::enable_shared_from_this<WsClient> {
public:
    // The function we'll call for each incoming WebSocket message
    using MessageCallback = std::function<void(const std::string&)>;

    WsClient(net::io_context& ioc,
             ssl::context&    ctx,
             MessageCallback  on_message);

    // Starts the async connection chain. Returns immediately —
    // actual work happens inside ioc.run() on the calling thread.
    void connect(const std::string& host,
                 const std::string& port,
                 const std::string& path);

private:
    // --- The 5-step async chain ---
    void on_resolve(beast::error_code ec, tcp::resolver::results_type results);
    void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep);
    void on_ssl_handshake(beast::error_code ec);
    void on_handshake(beast::error_code ec);
    void do_read();
    void on_read(beast::error_code ec, std::size_t bytes_transferred);

    // resolver_ turns "stream.binance.com" into an IP address
    tcp::resolver resolver_;

    // ws_ is the full layered stream: WebSocket over SSL over TCP
    // beast::tcp_stream    = TCP socket with timeouts
    // beast::ssl_stream<>  = TLS wrapper around tcp_stream
    // websocket::stream<>  = WebSocket framing on top of SSL
    websocket::stream<beast::ssl_stream<beast::tcp_stream>> ws_;

    // flat_buffer is Beast's growable buffer — accumulates bytes until a full
    // WebSocket frame arrives, then we flush it
    beast::flat_buffer buffer_;

    std::string     host_;
    std::string     path_;
    MessageCallback on_message_;
};
