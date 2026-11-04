#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

enum PlayerAttr : int {
    Attr_Health = 0,
    Attr_Mana = 1,
    Attr_Defense = 2,
    Attr_Count = 3
};

struct AttrValue {
    int32_t max = 0;
    int32_t current = 0;
};

struct PlayerState {
    std::string playerId;
    std::string playerName;

    std::array<AttrValue, Attr_Count> attributes;

    std::vector<uint16_t> hand;
    std::vector<uint16_t> deck;
    std::vector<uint16_t> discard;

    PlayerState() {
        attributes[Attr_Health] = {15, 15};
        attributes[Attr_Mana] = {10, 10};
        attributes[Attr_Defense] = {10, 10};
    }
};
