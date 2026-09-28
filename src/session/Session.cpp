#include "Session.h"

#include "logger/Logger.h"
#include "utils/Utils.h"

#include <cstdio>

using boost::asio::ip::tcp;

std::atomic<std::uint32_t> Session::s_nextId{1};

namespace {

const char* opcodeName(Protocol::Opcode op) {
    switch (op) {
        case Protocol::Opcode::CreateRoom: return "CreateRoom";
        case Protocol::Opcode::JoinRoom: return "JoinRoom";
        case Protocol::Opcode::ListRooms: return "ListRooms";
        case Protocol::Opcode::Error: return "Error";
        case Protocol::Opcode::CreateRoomResponse: return "CreateRoomResponse";
        case Protocol::Opcode::JoinRoomResponse: return "JoinRoomResponse";
        case Protocol::Opcode::ListRoomsResponse: return "ListRoomsResponse";
    }
    return "Unknown";
}

std::string opcodeStr(Protocol::Opcode op) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "0x%02X (%s)",
                  static_cast<unsigned>(op), opcodeName(op));
    return buf;
}

}

Session::Session(tcp::socket socket, RoomMap& rooms)
    : id_(s_nextId.fetch_add(1, std::memory_order_relaxed)),
      socket_(std::move(socket)),
      rooms_(rooms) {}

std::string Session::tag() const {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "SESSION:%04u", id_);
    return buf;
}

std::string Session::peerPrefix() const {
    return "[" + peerAddr_ + "] ";
}

void Session::logTrace(const std::string& m) const { Logger::trace(tag(), peerPrefix() + m); }
void Session::logDebug(const std::string& m) const { Logger::debug(tag(), peerPrefix() + m); }
void Session::logInfo(const std::string& m) const { Logger::info (tag(), peerPrefix() + m); }
void Session::logWarn(const std::string& m) const { Logger::warn (tag(), peerPrefix() + m); }
void Session::logError(const std::string& m) const { Logger::error(tag(), peerPrefix() + m); }

void Session::start() {
    boost::system::error_code ec;
    auto endpoint = socket_.remote_endpoint(ec);
    if (!ec) {
        peerAddr_ = endpoint.address().to_string() + ":" +
                    std::to_string(endpoint.port());
    }

    boost::system::error_code ndEc;
    // off Neigl algorithm
    socket_.set_option(boost::asio::ip::tcp::no_delay(true), ndEc);
    if (ndEc) {
        logWarn("Could not enable TCP_NODELAY: " + ndEc.message());
    }

    logInfo("Client connected");
    read();
}

void Session::read() {
    auto self = shared_from_this();
    socket_.async_read_some(
        boost::asio::buffer(recvChunk_),
        [self](boost::system::error_code ec, std::size_t length) {
            if (!ec) {
                if (length > 0) {
                    self->logTrace("<< recv " + std::to_string(length) +
                                   " bytes: " +
                                   Logger::hexDump(self->recvChunk_.data(), length));
                    self->recvBuffer_.insert(self->recvBuffer_.end(),
                                             self->recvChunk_.begin(),
                                             self->recvChunk_.begin() + length);
                    self->processBuffer();
                }
                self->read();
            } else if (ec == boost::asio::error::eof ||
                       ec == boost::asio::error::connection_reset)
            {
                self->logInfo("Client disconnected (remote closed connection)");
                self->onDisconnect();
            } else {
                self->logError("Read error: " + ec.message() +
                               " (code=" + std::to_string(ec.value()) + ")");
                self->onDisconnect();
            }
        });
}

void Session::processBuffer() {
    std::size_t offset = 0;
    Protocol::Opcode opcode{};
    std::vector<char> payload;

    while (Protocol::tryParsePacket(recvBuffer_, offset, opcode, payload)) {
        logDebug("<< recv opcode=" + opcodeStr(opcode) +
                 " payload=" + std::to_string(payload.size()) + "B");

        if (!payload.empty()) {
            logTrace("<< payload hex: " +
                     Logger::hexDump(payload.data(), payload.size()));
        }

        handleMessage(opcode, payload);
    }

    if (offset > 0) {
        recvBuffer_.erase(recvBuffer_.begin(), recvBuffer_.begin() + offset);
    }

    if (recvBuffer_.size() > 0) {
        logTrace("Buffered incomplete packet: " +
                 std::to_string(recvBuffer_.size()) + " bytes");
    }
}

void Session::send(const std::vector<char>& data) {
    boost::system::error_code ec;
    boost::asio::write(socket_, boost::asio::buffer(data), ec);
    if (ec) {
        logError("Write failed: " + ec.message() +
                 " (code=" + std::to_string(ec.value()) + ")");
    } else {
        logTrace(">> wire hex: " + Logger::hexDump(data.data(), data.size()));
    }
}

void Session::handleMessage(Protocol::Opcode opcode, const std::vector<char>& payload) {
    Protocol::Reader r(payload.data(), payload.size());

    switch (opcode) {
        case Protocol::Opcode::CreateRoom: handleCreateRoom(r); break;
        case Protocol::Opcode::JoinRoom: handleJoinRoom(r); break;
        case Protocol::Opcode::ListRooms: handleListRooms(); break;

        default: {
            logWarn("Unknown opcode received: " + opcodeStr(opcode));
            Protocol::Writer w;
            w.str("Unknown command");
            auto pkt = Protocol::buildPacket(Protocol::Opcode::Error, w);
            logInfo(">> send opcode=" + opcodeStr(Protocol::Opcode::Error) +
                    " payload=" + std::to_string(w.data().size()) + "B" +
                    " total=" + std::to_string(pkt.size()) + "B");
            send(pkt);
            break;
        }
    }
}

void Session::handleCreateRoom(Protocol::Reader& r) {
    std::string name = r.str();
    if (!r.ok() || name.empty()) {
        logWarn("CreateRoom: invalid player name");
        Protocol::Writer w;
        w.str("Invalid player name");
        auto pkt = Protocol::buildPacket(Protocol::Opcode::Error, w);
        logInfo(">> send opcode=" + opcodeStr(Protocol::Opcode::Error) +
                " total=" + std::to_string(pkt.size()) + "B");
        send(pkt);
        return;
    }
    setName(name);

    const std::string roomId = Utils::generateRoomId();
    const std::string clientIp = socket_.remote_endpoint().address().to_string();

    auto room = std::make_shared<Room>(roomId, clientIp);
    room->addPlayer(shared_from_this());
    rooms_[roomId] = room;
    setRoom(room);

    logInfo("Room created: id=" + roomId +
            " name=" + name +
            " host=" + clientIp +
            " (total rooms=" + std::to_string(rooms_.size()) + ")");

    Protocol::Writer w;
    w.str(roomId);
    auto pkt = Protocol::buildPacket(Protocol::Opcode::CreateRoomResponse, w);
    logInfo(">> send opcode=" + opcodeStr(Protocol::Opcode::CreateRoomResponse) +
            " payload=" + std::to_string(w.data().size()) + "B" +
            " total=" + std::to_string(pkt.size()) + "B");
    send(pkt);
}

void Session::handleJoinRoom(Protocol::Reader& r) {
    std::string roomId = r.str();
    std::string name = r.str();

    if (!r.ok() || roomId.empty() || name.empty()) {
        logWarn("JoinRoom: invalid parameters");
        Protocol::Writer w;
        w.str("Invalid parameters");
        auto pkt = Protocol::buildPacket(Protocol::Opcode::Error, w);
        logInfo(">> send opcode=" + opcodeStr(Protocol::Opcode::Error) +
                " total=" + std::to_string(pkt.size()) + "B");
        send(pkt);
        return;
    }
    setName(name);

    auto it = rooms_.find(roomId);
    if (it == rooms_.end()) {
        logWarn("JoinRoom: room not found id=" + roomId);
        Protocol::Writer w;
        w.str("Room not found");
        auto pkt = Protocol::buildPacket(Protocol::Opcode::Error, w);
        logInfo(">> send opcode=" + opcodeStr(Protocol::Opcode::Error) +
                " total=" + std::to_string(pkt.size()) + "B");
        send(pkt);
        return;
    }

    auto room = it->second;
    if (room->isFull()) {
        logWarn("JoinRoom: room full id=" + roomId +
                " players=" + std::to_string(room->playerCount()));
        Protocol::Writer w;
        w.str("Room is full");
        auto pkt = Protocol::buildPacket(Protocol::Opcode::Error, w);
        logInfo(">> send opcode=" + opcodeStr(Protocol::Opcode::Error) +
                " total=" + std::to_string(pkt.size()) + "B");
        send(pkt);
        return;
    }

    room->addPlayer(shared_from_this());
    setRoom(room);

    logInfo("Player joined room: id=" + roomId +
            " name=" + name +
            " players=" + std::to_string(room->playerCount()) +
            "/2");

    Protocol::Writer w;
    w.str(roomId);
    auto pkt = Protocol::buildPacket(Protocol::Opcode::JoinRoomResponse, w);
    logInfo(">> send opcode=" + opcodeStr(Protocol::Opcode::JoinRoomResponse) +
            " payload=" + std::to_string(w.data().size()) + "B" +
            " total=" + std::to_string(pkt.size()) + "B");
    send(pkt);
}

void Session::handleListRooms() {
    Protocol::Writer w;

    std::uint16_t count = 0;
    for (const auto& [id, room] : rooms_) {
        if (room->isFull()) continue;
        ++count;
    }

    w.u16(count);
    for (const auto& [id, room] : rooms_) {
        if (room->isFull()) continue;
        w.str(id);
        w.str(room->getHostIp());
        w.u8(static_cast<std::uint8_t>(room->playerCount()));
    }

    logInfo("ListRooms: returning " + std::to_string(count) +
            " available room(s), total=" + std::to_string(rooms_.size()));

    auto pkt = Protocol::buildPacket(Protocol::Opcode::ListRoomsResponse, w);
    logInfo(">> send opcode=" + opcodeStr(Protocol::Opcode::ListRoomsResponse) +
            " payload=" + std::to_string(w.data().size()) + "B" +
            " total=" + std::to_string(pkt.size()) + "B");
    send(pkt);
}

void Session::onDisconnect() {
    if (!currentRoom_) return;

    const std::string roomId = currentRoom_->getId();
    currentRoom_->removePlayer(shared_from_this());

    logInfo("Player removed from room: id=" + roomId +
            " name=" + playerName_ +
            " remaining=" + std::to_string(currentRoom_->playerCount()));

    if (currentRoom_->isEmpty()) {
        rooms_.erase(roomId);
        logInfo("Room deleted (empty): id=" + roomId +
                " total rooms=" + std::to_string(rooms_.size()));
    }
    currentRoom_.reset();
}