#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct PlayerState {
    std::string playerId;
    std::string playerName;
    int32_t healthMax = 15;
    int32_t healthCurrent = 15;
    int32_t manaMax = 10;
    int32_t manaCurrent = 10;
    int32_t defenseMax = 10;
    int32_t defenseCurrent = 10;
    std::vector<std::string> hand;
    std::vector<std::string> deck;
};
