#include "Protocol.h"

#include <stdexcept>

void Protocol::Writer::u8(uint8_t v) {
    buf_.push_back(static_cast<char>(v));
}

void Protocol::Writer::u16(uint16_t v) {
    buf_.push_back(static_cast<char>(v & 0xFF));
    buf_.push_back(static_cast<char>((v >> 8) & 0xFF));
}

void Protocol::Writer::u32(uint32_t v) {
    buf_.push_back(static_cast<char>(v & 0xFF));
    buf_.push_back(static_cast<char>((v >> 8) & 0xFF));
    buf_.push_back(static_cast<char>((v >> 16) & 0xFF));
    buf_.push_back(static_cast<char>((v >> 24) & 0xFF));
}

void Protocol::Writer::str(const std::string& s) {
    if (s.size() > kMaxStringLen) throw std::runtime_error("string too long");
    u16(static_cast<uint16_t>(s.size()));
    buf_.insert(buf_.end(), s.begin(), s.end());
}

uint8_t Protocol::Reader::u8() {
    if (pos_ + 1 > size_) { ok_ = false; return 0; }
    return static_cast<uint8_t>(data_[pos_++]);
}

uint16_t Protocol::Reader::u16() {
    if (pos_ + 2 > size_) { ok_ = false; return 0; }
    const uint16_t v =
        static_cast<uint8_t>(data_[pos_]) |
        (static_cast<uint16_t>(static_cast<uint8_t>(data_[pos_ + 1])) << 8);
    pos_ += 2;
    return v;
}

uint32_t Protocol::Reader::u32() {
    if (pos_ + 4 > size_) { ok_ = false; return 0; }
    const uint32_t v =
        static_cast<uint8_t>(data_[pos_]) |
        (static_cast<uint32_t>(static_cast<uint8_t>(data_[pos_ + 1])) << 8) |
        (static_cast<uint32_t>(static_cast<uint8_t>(data_[pos_ + 2])) << 16) |
        (static_cast<uint32_t>(static_cast<uint8_t>(data_[pos_ + 3])) << 24);
    pos_ += 4;
    return v;
}

std::string Protocol::Reader::str() {
    const uint16_t len = u16();
    if (!ok_ || pos_ + len > size_) { ok_ = false; return {}; }
    std::string s(data_ + pos_, len);
    pos_ += len;
    return s;
}

std::vector<char> Protocol::buildPacket(Opcode code,
                                        const std::vector<char>& payload)
{
    if (payload.size() > kMaxPayload) {
        throw std::runtime_error("payload too large");
    }

    const std::uint16_t length = static_cast<std::uint16_t>(1 + payload.size());

    std::vector<char> packet;
    packet.reserve(2 + length);
    packet.push_back(static_cast<char>(length & 0xFF));
    packet.push_back(static_cast<char>((length >> 8) & 0xFF));
    packet.push_back(static_cast<char>(code));
    packet.insert(packet.end(), payload.begin(), payload.end());
    return packet;
}

std::vector<char> Protocol::buildPacket(Opcode code, const Writer& w) {
    return buildPacket(code, w.data());
}

bool Protocol::tryParsePacket(const std::vector<char>& buffer,
                              std::size_t& offset,
                              Opcode& outOpcode,
                              std::vector<char>& outPayload) {
    if (buffer.size() - offset < 2) return false;

    const std::uint16_t length =
        static_cast<std::uint8_t>(buffer[offset]) |
        (static_cast<std::uint16_t>(
            static_cast<std::uint8_t>(buffer[offset + 1])) << 8);

    if (length < 1) return false;
    if (buffer.size() - offset < 2 + length) return false;

    outOpcode = static_cast<Opcode>(
        static_cast<std::uint8_t>(buffer[offset + 2]));
    outPayload.assign(buffer.begin() + offset + 3,
                      buffer.begin() + offset + 2 + length);

    offset += 2 + length;
    return true;
}