#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace Protocol {
    enum class Opcode : uint8_t {
        CreateRoom = 0x01,
        JoinRoom = 0x02,
        ListRooms = 0x03,

        Error = 0x80,
        CreateRoomResponse = 0x81,
        JoinRoomResponse = 0x82,
        ListRoomsResponse = 0x83,
    };
    
    std::string readString(const std::vector<char>& data, std::size_t& pos);
    
    std::vector<char> buildResponse(Opcode code, const std::string& payload = "");
}