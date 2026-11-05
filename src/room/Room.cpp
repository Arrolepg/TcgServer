#include "Room.h"

#include <algorithm>
#include <random>

#include "session/Session.h"
#include "protocol/Protocol.h"
#include "PlayerState.h"
#include "card/CardRegistry.h"
#include "logger/Logger.h"

namespace {
    struct AttrDef {
        const char* row;
        const char* shortName;
        PlayerAttr idx;
    };

    const AttrDef kAttrs[] = {
        {"health", "hp", Attr_Health},
        {"mana", "mana", Attr_Mana},
        {"defence", "def", Attr_Defense},
    };

    constexpr int32_t kInitialHandSize = 5;
    constexpr int32_t kDeckSize = 20;

    template<typename T>
    void shuffleDeck(std::vector<T>& deck) {
        static  std::random_device rd;
        static std::mt19937 gen(rd());
        std::shuffle(deck.begin(), deck.end(), gen);
    }

    void writeAttrs(Protocol::Writer& w, const PlayerState& p) {
        w.u16(std::size(kAttrs));
        for (const auto& a : kAttrs) {
            w.str(a.row);
            w.u32(static_cast<uint32_t>(p.attributes[a.idx].max));
            w.u32(static_cast<uint32_t>(p.attributes[a.idx].current));
        }
    }

    std::string formatAttrs(const PlayerState& p) {
        std::string out;
        for (const auto& a : kAttrs) {
            if (!out.empty()) out += ' ';
            out += a.shortName;
            out += '=';
            out += std::to_string(p.attributes[a.idx].current);
            out += '/';
            out += std::to_string(p.attributes[a.idx].max);
        }
        return out;
    }

    void writeHand(Protocol::Writer& w, const PlayerState& p) {
        w.u16(static_cast<uint16_t>(p.hand.size()));
        for (uint16_t id : p.hand) w.u16(id);
    }
    void writeDeck(Protocol::Writer& w, const PlayerState& p) {
        w.u16(static_cast<uint16_t>(p.deck.size()));
        for (uint16_t id : p.deck) w.u16(id);
    }
    void writeDiscard(Protocol::Writer& w, const PlayerState& p) {
        w.u16(static_cast<uint16_t>(p.discard.size()));
        for (uint16_t id : p.discard) w.u16(id);
    }
}

Room::Room(boost::asio::io_context& io, const std::string& id, const std::string& hostIp)
    : id_(id), hostIp_(hostIp), gameStarted_(false), turnTimer_(io) {}

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
    if (players_.empty()) {
        stopTurnTimer();
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
        std::vector<uint16_t> deck;
        deck.reserve(kDeckSize);
        const auto& pool = CardRegistry::allIds();
        for (int i = 0; i < kDeckSize; ++i) {
            deck.push_back(pool[i % pool.size()]);
        }
        shuffleDeck(deck);
        return deck;
    };

    hostState_ = PlayerState{};
    guestState_ = PlayerState{};

    PlayerState& p1 = hostState_;
    PlayerState& p2 = guestState_;

    p1.playerId = "P1";
    p1.playerName = hostSession_->getName();
    p1.deck = buildDeck();

    p2.playerId = "P2";
    p2.playerName = guestSession_->getName();
    p2.deck = buildDeck();

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

    firstPlayerId_ = (d1 >= d2) ? p1.playerId : p2.playerId;
    currentTurnPlayerId_ = firstPlayerId_;
    diceRoll_ = static_cast<uint8_t>(std::max(d1, d2));
    turnNumber_ = 1;
    gamePhase_ = Protocol::GamePhase::Playing;
    winnerId_.clear();
    stateSeq_ = 0;
    hasMatchState_ = true;

    Logger::info("ROOM:" + id_,
                 "Dice roll: P1=" + std::to_string(d1) +
                 " P2=" + std::to_string(d2) +
                 " -> first=" + firstPlayerId_ +
                 " (dice=" + std::to_string(diceRoll_) + ")");

    Logger::info("ROOM:" + id_,
                 "P1 name=" + p1.playerName + " " + formatAttrs(p1));
    Logger::info("ROOM:" + id_,
                 "P2 name=" + p2.playerName + " " + formatAttrs(p2));

    currentTurnTime_ = turnTimeLimit_;

    auto sendMatchStart = [&](std::shared_ptr<Session> who,
                              const PlayerState& me, const PlayerState& opp) {
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

    startTurnTimer();

    Logger::info("ROOM:" + id_, "Match start complete");
}

std::vector<char> Room::buildMatchStartPayload(const PlayerState& me, const PlayerState& opp) const {
    Protocol::Writer w;
    w.u32(Protocol::kWireVersion);
    w.u32(CardRegistry::version());

    w.str(id_);
    w.str(me.playerId);
    w.str(me.playerName);
    writeAttrs(w, me);
    writeHand (w, me);
    writeDeck (w, me);
    w.u16(static_cast<uint16_t>(me.hand.size() + me.discard.size()));

    w.str(opp.playerId);
    w.str(opp.playerName);
    writeAttrs(w, opp);
    w.u16(static_cast<uint16_t>(opp.hand.size()));
    w.u16(static_cast<uint16_t>(opp.hand.size() + opp.discard.size()));

    w.str(firstPlayerId_);
    w.u8(diceRoll_);

    w.u8(static_cast<uint8_t>(currentTurnTime_));
    w.u8(static_cast<uint8_t>(turnTimeLimit_));

    return w.data();
}

std::vector<char> Room::buildStateSyncPayload(const PlayerState& me, const PlayerState& opp) const {
    Protocol::Writer w;
    w.u32(Protocol::kWireVersion);
    w.u32(CardRegistry::version());
    w.u32(stateSeq_);

    w.str(id_);
    w.u8(static_cast<uint8_t>(gamePhase_));
    w.u8(turnNumber_);
    w.str(currentTurnPlayerId_);
    w.u8(diceRoll_);
    w.str(winnerId_);

    w.u8(static_cast<uint8_t>(currentTurnTime_));
    w.u8(static_cast<uint8_t>(turnTimeLimit_));

    w.str(me.playerId);
    w.str(me.playerName);
    writeAttrs(w, me);
    writeHand(w, me);
    writeDeck(w, me);
    writeDiscard(w, me);
    w.u16(static_cast<uint16_t>(me.hand.size() + me.discard.size()));

    w.str(opp.playerId);
    w.str(opp.playerName);
    writeAttrs(w, opp);
    w.u16(static_cast<uint16_t>(opp.hand.size()));
    w.u16(static_cast<uint16_t>(opp.deck.size()));
    writeDiscard(w, opp);
    w.u16(static_cast<uint16_t>(opp.hand.size() + opp.discard.size()));

    // change into next step
    w.u16(0);

    return w.data();
}

void Room::broadcastStateSync() {
    if (!hasMatchState_ || !hostSession_ || !guestSession_) return;

    ++stateSeq_;

    auto pushTo = [&](std::shared_ptr<Session> who, const PlayerState& me, const PlayerState& opp) {
        auto payload = buildStateSyncPayload(me, opp);
        auto pkt = Protocol::buildPacket(Protocol::Opcode::StateSync, payload);
        Logger::info("ROOM:" + id_,
                     ">> StateSync seq=" + std::to_string(stateSeq_) +
                     " to " + me.playerId +
                     " payload=" + std::to_string(payload.size()) + "B");
        who->send(pkt);
    };

    pushTo(hostSession_,  hostState_,  guestState_);
    pushTo(guestSession_, guestState_, hostState_);
}

void Room::startTurnTimer() {
    if (turnActive_) return;
    turnActive_ = true;
    scheduleNextTick();
}

void Room::scheduleNextTick() {
    std::weak_ptr<Room> weak = shared_from_this();
    turnTimer_.expires_after(std::chrono::seconds(1));
    turnTimer_.async_wait([weak](const boost::system::error_code& ec) {
        if (auto self = weak.lock()) {
            self->onTurnTick(ec);
        }
    });
}

void Room::onTurnTick(const boost::system::error_code& ec) {
    if (ec == boost::asio::error::operation_aborted) return;
    if (ec) return;
    if (!turnActive_) return;
    if (!hasMatchState_ || gamePhase_ != Protocol::GamePhase::Playing) {
        stopTurnTimer();
        return;
    }
    if (currentTurnTime_ > 0) --currentTurnTime_;
    if (currentTurnTime_ == 0) {
        endTurn();
    } else {
        broadcastStateSync();
    }

    scheduleNextTick();
}

void Room::stopTurnTimer() {
    turnActive_ = false;
    try {
        turnTimer_.cancel();
    } catch (const boost::system::system_error& ) {

    }
}

void Room::endTurn() {
    if (!hasMatchState_) return;
    if (gamePhase_ != Protocol::GamePhase::Playing) return;

    currentTurnPlayerId_ = (currentTurnPlayerId_ == "P1") ? "P2" : "P1";
    ++turnNumber_;
    currentTurnTime_ = turnTimeLimit_;

    Logger::info("ROOM:" + id_,
        "Turn -> " + currentTurnPlayerId_ + " (turn #" + std::to_string(turnNumber_) + ")");

    broadcastStateSync();
}

void Room::handleEndTurn(const std::shared_ptr<Session> &caller) {
    if (!hasMatchState_ || gamePhase_ != Protocol::GamePhase::Playing) return;

    std::string callerId;
    if (caller == hostSession_) callerId = "P1";
    else if (caller == guestSession_) callerId = "P2";
    else return;

    if (callerId != currentTurnPlayerId_) {
        Logger::debug("ROOM:" + id_,
            "EndTurn ignored: caller = " + callerId + " current = " + currentTurnPlayerId_);
        return;
    }
    endTurn();
}
