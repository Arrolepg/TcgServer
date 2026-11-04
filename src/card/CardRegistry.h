#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <utility>

class CardRegistry {
public:
    static const std::vector<std::pair<uint16_t, std::string>>& all();
    static const std::vector<uint16_t>& allIds();

    static const std::string& nameOf(uint16_t id);
    static bool tryNameOf(uint16_t id, std::string& out);
    static uint16_t idOf(const std::string& rowName);
    static bool tryIdOf(const std::string& rowName, uint16_t& out);

    static uint32_t version();
};
