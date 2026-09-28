#pragma once
#include <cstdint>
#include <string>
#include <vector>

class Protocol {
public:
    enum class Opcode : uint8_t {
        CreateRoom = 0x01,
        JoinRoom = 0x02,
        ListRooms = 0x03,

        Error = 0x80,
        CreateRoomResponse = 0x81,
        JoinRoomResponse = 0x82,
        ListRoomsResponse = 0x83,
    };

    static constexpr std::size_t kHeaderSize = 3;
    static constexpr std::size_t kMaxPayload = 65533;
    static constexpr std::size_t kMaxStringLen = 65535;

    class Writer {
    public:
        void u8(uint8_t v);
        void u16(uint16_t v);
        void u32(uint32_t v);
        void str(const std::string& s);

        const std::vector<char>& data() const { return buf_; }

    private:
        std::vector<char> buf_;
    };

    class Reader {
    public:
        Reader(const char* data, std::size_t size)
            : data_(data), size_(size) {}

        uint8_t u8();
        uint16_t u16();
        uint32_t u32();
        std::string str();

        bool ok()    const { return ok_; }
        bool empty() const { return pos_ >= size_; }

    private:
        const char* data_;
        std::size_t size_;
        std::size_t pos_ = 0;
        bool ok_ = true;
    };

    Protocol() = delete;
    Protocol(const Protocol&) = delete;
    Protocol& operator=(const Protocol&) = delete;

    static std::vector<char> buildPacket(Opcode code, const std::vector<char>& payload);
    static std::vector<char> buildPacket(Opcode code, const Writer& w);

    static bool tryParsePacket(const std::vector<char>& buffer,
                               std::size_t& offset,
                               Opcode& outOpcode,
                               std::vector<char>& outPayload);
};