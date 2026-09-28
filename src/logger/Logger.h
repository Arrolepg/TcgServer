#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>

class Logger {
public:
    enum class Level : uint8_t {
        Trace = 0,
        Debug = 1,
        Info = 2,
        Warn = 3,
        Error = 4,
        Fatal = 5,
    };

    Logger() = delete;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    static void  setMinLevel(Level level);
    static Level getMinLevel();

    static void log(Level level,
                    const std::string& component,
                    const std::string& message);

    static void trace(const std::string& component, const std::string& message);
    static void debug(const std::string& component, const std::string& message);
    static void info (const std::string& component, const std::string& message);
    static void warn (const std::string& component, const std::string& message);
    static void error(const std::string& component, const std::string& message);
    static void fatal(const std::string& component, const std::string& message);

    static std::string hexDump(const void* data,
                               std::size_t size,
                               std::size_t maxBytes = 64);

    static const char* levelToString(Level level);
    static Level parseLevel(const std::string& s);

private:
    static std::atomic<Level> s_minLevel;
    static std::mutex s_mutex;

    static void formatTimestamp(char* buf, std::size_t n);
    static unsigned short currentThreadId();
};