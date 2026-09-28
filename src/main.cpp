#include <boost/asio.hpp>
#include <cstdlib>
#include <exception>
#include <iostream>

#include "config/Config.h"
#include "logger/Logger.h"
#include "server/Server.h"

#ifdef _WIN32
#include <windows.h>
#endif

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    Config::ServerConfig cfg;
    const std::string cfgPath = Config::findDefaultConfigPath();
    const bool loaded = Config::load(cfgPath, cfg);

    const char* envLevel = std::getenv("TCG_LOG_LEVEL");
    const std::string levelStr = envLevel ? envLevel : cfg.logLevel;

    Logger::setMinLevel(Logger::parseLevel(levelStr));

    if (loaded) {
        Logger::info("MAIN", "Config loaded from: " + cfgPath);
    } else {
        Logger::warn("MAIN", "config.ini not found at '" + cfgPath +
                             "', using defaults");
    }
    Logger::info("MAIN", "Log level: " + std::string(Logger::levelToString(Logger::getMinLevel())) +
                         (envLevel ? " (from ENV)" : " (from config)"));
    Logger::info("MAIN", "Port: " + std::to_string(cfg.port));

    try {
        boost::asio::io_context io;
        Server server(io, static_cast<short>(cfg.port));
        io.run();
    } catch (const std::exception& e) {
        Logger::fatal("MAIN", std::string("Unhandled exception: ") + e.what());
        return 1;
    }
    return 0;
}