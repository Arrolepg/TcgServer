#include "Logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>

namespace Logger {
    void log(const std::string& msg) {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);

        std::tm tmBuf{};
#ifdef _WIN32
        localtime_s(&tmBuf, &time);
#else
        localtime_r(&time, &tmBuf);
#endif

        std::cout << "[" << std::put_time(&tmBuf, "%Y-%m-%d %H:%M:%S") << "] " << msg << std::endl;
    }
}