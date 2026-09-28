#pragma once
#include <string>

class Utils {
public:
    Utils() = delete;
    Utils(const Utils&) = delete;
    Utils& operator=(const Utils&) = delete;

    static std::string ansiToUtf8(const std::string& ansi);
    static std::string generateRoomId();
};