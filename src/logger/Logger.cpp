#include "Logger.h"

#include <chrono>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

std::atomic<Logger::Level> Logger::s_minLevel{Logger::Level::Info};
std::mutex Logger::s_mutex;

void Logger::setMinLevel(Level level) {
    s_minLevel.store(level, std::memory_order_relaxed);
}

Logger::Level Logger::getMinLevel() {
    return s_minLevel.load(std::memory_order_relaxed);
}

const char* Logger::levelToString(Level level) {
    switch (level) {
        case Level::Trace: return "TRACE";
        case Level::Debug: return "DEBUG";
        case Level::Info: return "INFO ";
        case Level::Warn: return "WARN ";
        case Level::Error: return "ERROR";
        case Level::Fatal: return "FATAL";
    }
    return "?????";
}

Logger::Level Logger::parseLevel(const std::string& s) {
    std::string t;
    t.reserve(s.size());
    for (char c : s) {
        t += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (t == "trace") return Level::Trace;
    if (t == "debug") return Level::Debug;
    if (t == "info") return Level::Info;
    if (t == "warn" || t == "warning") return Level::Warn;
    if (t == "error") return Level::Error;
    if (t == "fatal") return Level::Fatal;
    return Level::Info;
}

void Logger::formatTimestamp(char* buf, std::size_t n) {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto t = system_clock::to_time_t(now);
    const auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    std::tm tmBuf{};
#ifdef _WIN32
    localtime_s(&tmBuf, &t);
#else
    localtime_r(&t, &tmBuf);
#endif

    std::snprintf(buf, n, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
                  tmBuf.tm_year + 1900, tmBuf.tm_mon + 1, tmBuf.tm_mday,
                  tmBuf.tm_hour, tmBuf.tm_min, tmBuf.tm_sec,
                  static_cast<int>(ms.count()));
}

unsigned short Logger::currentThreadId() {
    std::hash<std::thread::id> h;
    return static_cast<unsigned short>(h(std::this_thread::get_id()) & 0xFFFF);
}

void Logger::log(Level level,
                 const std::string& component,
                 const std::string& message) {
    if (level < s_minLevel.load(std::memory_order_relaxed)) return;

    char ts[32];
    formatTimestamp(ts, sizeof(ts));
    const unsigned short tid = currentThreadId();

    std::lock_guard<std::mutex> lock(s_mutex);
    std::cout
        << ts
        << " [" << levelToString(level) << "]"
        << " [T:" << std::setw(4) << std::setfill('0') << tid << "]"
        << " [" << component << "]"
        << " " << message
        << std::endl;
}

void Logger::trace(const std::string& c, const std::string& m) { log(Level::Trace, c, m); }
void Logger::debug(const std::string& c, const std::string& m) { log(Level::Debug, c, m); }
void Logger::info(const std::string& c, const std::string& m) { log(Level::Info,  c, m); }
void Logger::warn(const std::string& c, const std::string& m) { log(Level::Warn,  c, m); }
void Logger::error(const std::string& c, const std::string& m) { log(Level::Error, c, m); }
void Logger::fatal(const std::string& c, const std::string& m) { log(Level::Fatal, c, m); }

std::string Logger::hexDump(const void* data, std::size_t size, std::size_t maxBytes) {
    const auto* p = static_cast<const unsigned char*>(data);
    const std::size_t n = (size < maxBytes) ? size : maxBytes;

    std::ostringstream out;
    out << std::hex << std::uppercase << std::setfill('0');
    for (std::size_t i = 0; i < n; ++i) {
        out << std::setw(2) << static_cast<unsigned>(p[i]);
        if (i + 1 < n) out << ' ';
    }
    if (size > maxBytes) {
        out << " ... (+" << std::dec << (size - maxBytes) << " bytes)";
    }
    return out.str();
}