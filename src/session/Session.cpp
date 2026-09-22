#include "Session.h"

#include "logger/Logger.h"
#include "protocol/Protocol.h"
#include "utils/Utils.h"

using boost::asio::ip::tcp;

Session::Session(tcp::socket socket, RoomMap& rooms)
    : socket_(std::move(socket)), rooms_(rooms) {}

void Session::start() {
    boost::system::error_code ec;
    auto endpoint = socket_.remote_endpoint(ec);
    if (!ec) {
        Logger::log("Client connected: " + endpoint.address().to_string() +
                    ":" + std::to_string(endpoint.port()));
    } else {
        Logger::log("Client connected (endpoint unavailable)");
    }
    read();
}

void Session::send(const std::vector<char>& data) {
    boost::system::error_code ec;
    boost::asio::write(socket_, boost::asio::buffer(data), ec);
    if (ec) {
        Logger::log("Write error: " + Utils::ansiToUtf8(ec.message()));
    } else {
        Logger::log("Written " + std::to_string(data.size()) + " bytes");
    }
}

void Session::read() {
    auto self = shared_from_this();
    socket_.async_read_some(
        boost::asio::buffer(data_),
        [self](boost::system::error_code ec, std::size_t length) {
            if (!ec) {
                std::vector<char> msg(self->data_.begin(),
                                      self->data_.begin() + length);
                self->handleMessage(msg);
                self->read();
            } else {
                Logger::log("Read error: " + Utils::ansiToUtf8(ec.message()) +
                            " from " + self->playerName_);
                self->onDisconnect();
            }
        });
}

void Session::handleMessage(const std::vector<char>& msg) {
    if (msg.empty()) return;

    auto opcode = static_cast<Protocol::Opcode>(static_cast<uint8_t>(msg[0]));
    std::vector<char> payload(msg.begin() + 1, msg.end());

    Logger::log("Received opcode: " +
                std::to_string(static_cast<int>(opcode)) +
                ", size: " + std::to_string(payload.size()));

    switch (opcode) {
        case Protocol::Opcode::CreateRoom:handleCreateRoom(payload); break;
        case Protocol::Opcode::JoinRoom:handleJoinRoom(payload); break;
        case Protocol::Opcode::ListRooms:handleListRooms(); break;
        default:
            Logger::log("Unknown opcode: " +
                        std::to_string(static_cast<int>(opcode)));
            send(Protocol::buildResponse(Protocol::Opcode::Error,
                                         "Unknown command"));
            break;
    }
}

void Session::handleCreateRoom(const std::vector<char>& payload) {
    std::size_t pos = 0;
    std::string name = Protocol::readString(payload, pos);
    if (name.empty()) {
        send(Protocol::buildResponse(Protocol::Opcode::Error,
                                     "Invalid player name"));
        return;
    }
    setName(name);

    const std::string roomId = Utils::generateRoomId();
    const std::string clientIp = socket_.remote_endpoint().address().to_string();

    auto room = std::make_shared<Room>(roomId, clientIp);
    room->addPlayer(shared_from_this());
    rooms_[roomId] = room;
    setRoom(room);

    Logger::log("Room created: " + roomId + " by " + name + " at " + clientIp);
    send(Protocol::buildResponse(Protocol::Opcode::CreateRoomResponse, roomId));
}

void Session::handleJoinRoom(const std::vector<char>& payload) {
    std::size_t pos = 0;
    std::string roomId = Protocol::readString(payload, pos);
    std::string name = Protocol::readString(payload, pos);

    if (roomId.empty() || name.empty()) {
        send(Protocol::buildResponse(Protocol::Opcode::Error,
                                     "Invalid parameters"));
        return;
    }
    setName(name);

    auto it = rooms_.find(roomId);
    if (it == rooms_.end()) {
        send(Protocol::buildResponse(Protocol::Opcode::Error,
                                     "Room not found"));
        return;
    }

    auto room = it->second;
    if (room->isFull()) {
        send(Protocol::buildResponse(Protocol::Opcode::Error, "Room is full"));
        return;
    }

    room->addPlayer(shared_from_this());
    setRoom(room);

    Logger::log("Player " + name + " joined room " + roomId);
    send(Protocol::buildResponse(Protocol::Opcode::JoinRoomResponse, roomId));
}

void Session::handleListRooms() {
    std::string roomList;
    for (const auto& [roomId, room] : rooms_) {
        if (room->isFull()) continue;
        if (!roomList.empty()) roomList += ";";
        roomList += roomId + "," + room->getHostIp() + "," +
                    std::to_string(room->playerCount());
    }
    Logger::log("Sending room list: " + roomList);
    send(Protocol::buildResponse(Protocol::Opcode::ListRoomsResponse, roomList));
}

void Session::onDisconnect() {
    if (!currentRoom_) return;

    currentRoom_->removePlayer(shared_from_this());
    Logger::log("Player " + playerName_ + " removed from room " +
                currentRoom_->getId());

    if (currentRoom_->isEmpty()) {
        const std::string roomId = currentRoom_->getId();
        rooms_.erase(roomId);
        Logger::log("Room " + roomId + " deleted (empty)");
    }
    currentRoom_.reset();
}