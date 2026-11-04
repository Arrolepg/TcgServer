#include "CardRegistry.h"
#include <stdexcept>
#include <unordered_map>

namespace {
    const std::vector<std::pair<uint16_t, std::string>> kCards = {
        {1, "Fireball"},
        {2, "Arcane"},
        {3, "Frostbind"},
        {4, "BladeRush"},
        {5, "CrushingBlow"},
        {6, "PiercingShot"},
        {7, "IronWall"},
        {8, "CounterStance"},
        {9, "BarrierCharm"}
    };

    const std::unordered_map<uint16_t, std::string>& idToName() {
        static const auto m = []() {
            std::unordered_map<uint16_t, std::string> x;
            for (auto& [id, n] : kCards) {
                x.emplace(id, n);
            }
            return x;
        }();
        return m;
    }

    const std::unordered_map<std::string, uint16_t>& nameToId() {
        static const auto m = []() {
            std::unordered_map<std::string, uint16_t> x;
            for (auto& [id, n] : kCards) {
                x.emplace(n, id);
            }
            return x;
        }();
        return m;
    }
}

const std::vector<std::pair<uint16_t, std::string>>& CardRegistry::all() {
    return kCards;
}

const std::vector<uint16_t>& CardRegistry::allIds() {
    static const std::vector<uint16_t> v = []() {
        std::vector<uint16_t> r;
        r.reserve(kCards.size());
        for (auto& [id, _] : kCards) r.push_back(id);
        return r;
    }();
    return v;
}

const std::string& CardRegistry::nameOf(uint16_t id) {
    auto it = idToName().find(id);
    if (it == idToName().end())
        throw std::out_of_range("CardRegistry: unknown id " + std::to_string(id));
    return it->second;
}

bool CardRegistry::tryNameOf(uint16_t id, std::string& out) {
    auto it = idToName().find(id);
    if (it == idToName().end()) return false;
    out = it->second;
    return true;
}

uint16_t CardRegistry::idOf(const std::string& rowName) {
    auto it = nameToId().find(rowName);
    if (it == nameToId().end())
        throw std::out_of_range("CardRegistry: unknown name " + rowName);
    return it->second;
}

bool CardRegistry::tryIdOf(const std::string& rowName, uint16_t& out) {
    auto it = nameToId().find(rowName);
    if (it == nameToId().end()) return false;
    out = it->second;
    return true;
}

uint32_t CardRegistry::version() {
    static const uint32_t v = []() {
        uint32_t h = 2166136261u;
        for (auto& [id, name] : kCards) {
            for (unsigned char c : name) {
                h ^= c;
                h *= 16777619u;
            }
            h ^= (id & 0xFFu);
            h *= 16777619u;
            h ^= ((id >> 8) & 0xFFu);
            h *= 16777619u;
        }
        return h;
    }();
    return v;
}