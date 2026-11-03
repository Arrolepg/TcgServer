#include "Room.h"

#include <algorithm>
#include <random>

#include "session/Session.h"
#include "protocol/Protocol.h"
#include "PlayerState.h"
#include "logger/Logger.h"

namespace {
    const std::vector<std::string> kCardPool = {
        "Fireball",
        "Arcane",
        "Frostbind",
        "BladeRush",
        "CrushingBlow",
        "PiercingShot",
        "IronWall",
        "CounterStance",
        "BarrierCharm"
    };

    struct AttrInt {
        const char* row;
        int32_t max;
    };

    const AttrInt kAttrDefaults[] = {
        {"health", 15},
        {"mana", 10},
        {"defence", 10}
    };

    constexpr int32_t kInitialHandSize = 5;
    constexpr int32_t kDeckSize = 20;

    void shuffleDeck(std::vector<std::string>& deck) {
        static  std::random_device rd;
        static std::mt19937 gen(rd());
        std::shuffle(deck.begin(), deck.end(), gen);
    }
}

Room::Room(const std::string& id, const std::string& hostIp)
    : id_(id), hostIp_(hostIp), gameStarted_(false) {}

void Room::addPlayer(const std::shared_ptr<Session>& player) {
    if (!hostSession_) {
        hostSession_ = player;
    } else if (!guestSession_ && player != hostSession_) {
        guestSession_ = player;
    }
    players_.insert(player);
}

void Room::removePlayer(const std::shared_ptr<Session>& player) {
    players_.erase(player);
    if (player == hostSession_) {
        hostSession_.reset();
    }
    if (player == guestSession_) {
        guestSession_.reset();
    }
}

bool Room::isFull() const {
    return players_.size() >= 2;
}
bool Room::isEmpty() const {
    return players_.empty();
}

void Room::initializeGame() {
    if (gameStarted_) {
        Logger::debug("ROOM:" + id_, "initializeGame skipped (already started)");
        return;
    }
    if (!hostSession_ || !guestSession_) {
        Logger::debug("ROOM:" + id_, "initializeGame skipped (not enough players)");
        return;
    }

    gameStarted_ = true;

    Logger::info("ROOM:" + id_,
                 "Starting match: players=2 host=" + hostSession_->getName() +
                 " guest=" + guestSession_->getName());

    auto buildDeck = []() {
        std::vector<std::string> deck;
        deck.reserve(kDeckSize);
        for (int i = 0; i < kDeckSize; ++i) {
            deck.push_back(kCardPool[i % kCardPool.size()]);
        }
        shuffleDeck(deck);
        return deck;
    };

    PlayerState p1, p2;

    p1.playerId = "P1";
    p1.playerName = hostSession_->getName();
    p1.deck = buildDeck();
    for (auto& a : kAttrDefaults) {
        if (a.row == std::string("health")) {
            p1.healthMax = a.max;
            p1.healthCurrent = a.max;
        }
        if (a.row == std::string("mana")) {
            p1.manaMax = a.max;
            p1.manaCurrent = a.max;
        }
        if (a.row == std::string("defence")) {
            p1.defenseMax = a.max;
            p1.defenseCurrent = a.max;
        }
    }

    p2.playerId = "P2";
    p2.playerName = guestSession_->getName();
    p2.deck = buildDeck();
    for (auto& a : kAttrDefaults) {
        if (a.row == std::string("health")) {
            p2.healthMax = a.max;
            p2.healthCurrent = a.max;
        }
        if (a.row == std::string("mana")) {
            p2.manaMax = a.max;
            p2.manaCurrent = a.max;
        }
        if (a.row == std::string("defence")) {
            p2.defenseMax = a.max;
            p2.defenseCurrent = a.max;
        }
    }

    Logger::debug("ROOM:" + id_,
                  "Decks built: P1=" + std::to_string(p1.deck.size()) +
                  " P2=" + std::to_string(p2.deck.size()) + " cards each");

    for (int i = 0; i < kInitialHandSize; ++i) {
        p1.hand.push_back(p1.deck.back());
        p1.deck.pop_back();

        p2.hand.push_back(p2.deck.back());
        p2.deck.pop_back();
    }

    Logger::debug("ROOM:" + id_,
                  "Hands dealt: P1 hand=" + std::to_string(p1.hand.size()) +
                  " deck=" + std::to_string(p1.deck.size()) +
                  ", P2 hand=" + std::to_string(p2.hand.size()) +
                  " deck=" + std::to_string(p2.deck.size()));

    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dice(1, 6);
    int d1 = dice(gen), d2 = dice(gen);
    const std::string first = (d1 >= d2) ? p1.playerId : p2.playerId;
    const int diceRoll = std::max(d1, d2);

    Logger::info("ROOM:" + id_,
                 "Dice roll: P1=" + std::to_string(d1) +
                 " P2=" + std::to_string(d2) +
                 " -> first=" + first +
                 " (dice=" + std::to_string(diceRoll) + ")");

    Logger::info("ROOM:" + id_,
                 "P1 name=" + p1.playerName +
                 " hp=" + std::to_string(p1.healthCurrent) + "/" + std::to_string(p1.healthMax) +
                 " mana=" + std::to_string(p1.manaCurrent) + "/" + std::to_string(p1.manaMax) +
                 " def=" + std::to_string(p1.defenseCurrent) + "/" + std::to_string(p1.defenseMax));
    Logger::info("ROOM:" + id_,
                 "P2 name=" + p2.playerName +
                 " hp=" + std::to_string(p2.healthCurrent) + "/" + std::to_string(p2.healthMax) +
                 " mana=" + std::to_string(p2.manaCurrent) + "/" + std::to_string(p2.manaMax) +
                 " def=" + std::to_string(p2.defenseCurrent) + "/" + std::to_string(p2.defenseMax));

    auto buildMatchStartPayload = [&](const PlayerState& me, const PlayerState& opp) -> std::vector<char> {
            Protocol::Writer w;
            w.str(id_);
            w.str(me.playerId);
            w.str(me.playerName);

            w.u16(std::size(kAttrDefaults));
            for (auto& a : kAttrDefaults) {
                w.str(a.row);
                int maxV = 0, curV = 0;
                std::string r = a.row;
                if (r == "health") {
                    maxV = me.healthMax;
                    curV = me.healthCurrent;
                }
                if (r == "mana") {
                    maxV = me.manaMax;
                    curV = me.manaCurrent;
                }
                if (r == "defence") {
                    maxV = me.defenseMax;
                    curV = me.defenseCurrent;
                }
                w.u32(static_cast<uint32_t>(maxV));
                w.u32(static_cast<uint32_t>(curV));
            }

            w.u16(static_cast<uint16_t>(me.hand.size()));
            for (auto& c : me.hand) w.str(c);

            w.u16(static_cast<uint16_t>(me.deck.size()));
            for (auto& c : me.deck) w.str(c);

            w.str(opp.playerId);
            w.str(opp.playerName);

            w.u16(std::size(kAttrDefaults));
            for (auto& a : kAttrDefaults) {
                w.str(a.row);
                int maxV = 0, curV = 0;
                std::string r = a.row;
                if (r == "health") {
                    maxV = opp.healthMax;
                    curV = opp.healthCurrent;
                }
                if (r == "mana") {
                    maxV = opp.manaMax;
                    curV = opp.manaCurrent;
                }
                if (r == "defence") {
                    maxV = opp.defenseMax;
                    curV = opp.defenseCurrent;
                }
                w.u32(static_cast<uint32_t>(maxV));
                w.u32(static_cast<uint32_t>(curV));
            }

            w.u16(static_cast<uint16_t>(opp.hand.size()));

            w.str(first);
            w.u8(static_cast<uint8_t>(diceRoll));

            return w.data();
        };

    auto sendMatchStart = [&](std::shared_ptr<Session> who, const PlayerState& me, const PlayerState& opp) {
        auto payload = buildMatchStartPayload(me, opp);
        auto pkt = Protocol::buildPacket(Protocol::Opcode::MatchStart, payload);

        Logger::info("ROOM:" + id_,
                     ">> MatchStart to " + me.playerId + " (" + me.playerName + ")" +
                     " payload=" + std::to_string(payload.size()) + "B" +
                     " total=" + std::to_string(pkt.size()) + "B");

        who->send(pkt);
    };

    sendMatchStart(hostSession_, p1, p2);
    sendMatchStart(guestSession_, p2, p1);

    Logger::info("ROOM:" + id_, "Match start complete");
}
