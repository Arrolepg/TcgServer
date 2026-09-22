#pragma once
#include <boost/asio.hpp>

#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "room/Room.h"

class Session : public std::enable_shared_from_this<Session> {
public:
    using RoomMap = std::map<std::string, std::shared_ptr<Room>>;

    Session(boost::asio::ip::tcp::socket socket, RoomMap& rooms);

    void start();
    void send(const std::vector<char>& data);

    void setRoom(const std::shared_ptr<Room>& room) {
        currentRoom_ = room;
    }
    std::shared_ptr<Room> getRoom() const {
        return currentRoom_;
    }

    const std::string& getName() const {
        return playerName_;
    }
    void setName(const std::string& name) {
        playerName_ = name;
    }

private:
    void read();
    void handleMessage(const std::vector<char>& msg);

    void handleCreateRoom(const std::vector<char>& payload);
    void handleJoinRoom(const std::vector<char>& payload);
    void handleListRooms();

    void onDisconnect();

    boost::asio::ip::tcp::socket socket_;
    std::array<char, 4096> data_{};
    std::shared_ptr<Room> currentRoom_;
    std::string playerName_ = "Unknown";
    RoomMap& rooms_;
};