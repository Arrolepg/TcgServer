#pragma once
#include <memory>
#include <set>
#include <string>

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

private:
    std::string id_;
    std::string hostIp_;
    std::set<std::shared_ptr<Session>> players_;
};