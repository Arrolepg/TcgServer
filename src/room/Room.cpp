#include "Room.h"

Room::Room(const std::string& id, const std::string& hostIp)
    : id_(id), hostIp_(hostIp) {}

void Room::addPlayer(const std::shared_ptr<Session>& player) {
    players_.insert(player);
}

void Room::removePlayer(const std::shared_ptr<Session>& player) {
    players_.erase(player);
}

bool Room::isFull() const {
    return players_.size() >= 2;
}
bool Room::isEmpty() const {
    return players_.empty();
}