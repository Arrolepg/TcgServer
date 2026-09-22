#pragma once
#include <string>

namespace Utils {
    std::string ansiToUtf8(const std::string& ansi);
    std::string generateRoomId();
}