#include "Server.h"

#include "logger/Logger.h"
#include "session/Session.h"

using boost::asio::ip::tcp;

Server::Server(boost::asio::io_context& io, short port)
    : acceptor_(io, tcp::endpoint(tcp::v4(), port)) {
    Logger::info("SERVER", "Listening on 0.0.0.0:" + std::to_string(port));
    accept();
}

void Server::accept() {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec) {
                auto session = std::make_shared<Session>(std::move(socket), rooms_);
                session->start();
            } else {
                Logger::error("SERVER", "Accept failed: " + ec.message() +
                              " (code=" + std::to_string(ec.value()) + ")");
            }
            accept();
        });
}