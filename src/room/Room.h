#pragma once
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "PlayerState.h"
#include "protocol/Protocol.h"

class Session;

class Room {
public:
    Room(const std::string& id, const std::string& hostIp);

    const std::string& getId() const {
        return id_;
    }
    const std::string& getHostIp() const {
        return hostIp_;
    }

    const std::set<std::shared_ptr<Session>>& getPlayers() const {
        return players_;
    }

    void addPlayer(const std::shared_ptr<Session>& player);
    void removePlayer(const std::shared_ptr<Session>& player);

    bool isFull() const;
    bool isEmpty() const;
    std::size_t playerCount() const {
        return players_.size();
    }

    void initializeGame();

    void broadcastStateSync();

    void sendCurrentStateTo(const std::shared_ptr<Session>& to);

private:
    std::vector<char> buildMatchStartPayload(const PlayerState& me, const PlayerState& opp) const;
    std::vector<char> buildStateSyncPayload(const PlayerState& me, const PlayerState& opp) const;

    std::string id_;
    std::string hostIp_;
    std::set<std::shared_ptr<Session>> players_;

    std::shared_ptr<Session> hostSession_;
    std::shared_ptr<Session> guestSession_;
    bool gameStarted_;

    PlayerState hostState_;
    PlayerState guestState_;
    bool hasMatchState_ = false;

    uint32_t stateSeq_ = 0;
    uint8_t turnNumber_ = 0;
    uint8_t diceRoll_ = 0;
    Protocol::GamePhase gamePhase_ = Protocol::GamePhase::Init;
    std::string firstPlayerId_;
    std::string currentTurnPlayerId_;
    std::string winnerId_;
};