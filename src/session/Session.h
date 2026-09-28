#pragma once
#include <boost/asio.hpp>

#include <array>
#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "protocol/Protocol.h"
#include "room/Room.h"

class Session : public std::enable_shared_from_this<Session> {
public:
    using RoomMap = std::map<std::string, std::shared_ptr<Room>>;

    Session(boost::asio::ip::tcp::socket socket, RoomMap& rooms);

    void start();
    void send(const std::vector<char>& data);

    void setRoom(const std::shared_ptr<Room>& room) { currentRoom_ = room; }
    std::shared_ptr<Room> getRoom() const { return currentRoom_; }

    const std::string& getName() const { return playerName_; }
    void setName(const std::string& name) { playerName_ = name; }

private:
    void read();
    void processBuffer();
    void handleMessage(Protocol::Opcode opcode, const std::vector<char>& payload);

    void handleCreateRoom(Protocol::Reader& r);
    void handleJoinRoom(Protocol::Reader& r);
    void handleListRooms();

    void onDisconnect();

    std::string tag() const;
    std::string peerPrefix() const;
    void logInfo(const std::string& m) const;
    void logDebug(const std::string& m) const;
    void logWarn(const std::string& m) const;
    void logError(const std::string& m) const;
    void logTrace(const std::string& m) const;

    static std::atomic<std::uint32_t> s_nextId;
    const std::uint32_t id_;
    std::string peerAddr_ = "<unknown>";

    boost::asio::ip::tcp::socket socket_;
    std::array<char, 4096> recvChunk_{};
    std::vector<char> recvBuffer_;
    std::shared_ptr<Room> currentRoom_;
    std::string playerName_ = "Unknown";
    RoomMap& rooms_;
};