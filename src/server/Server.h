#pragma once
#include <boost/asio.hpp>

#include <map>
#include <memory>
#include <string>

#include "room/Room.h"

class Server {
public:
    Server(boost::asio::io_context& io, short port);

private:
    void accept();

    boost::asio::ip::tcp::acceptor acceptor_;
    std::map<std::string, std::shared_ptr<Room>> rooms_;
};