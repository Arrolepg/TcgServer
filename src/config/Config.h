#pragma once
#include <cstdint>
#include <string>

class Config {
public:
    struct ServerConfig {
        std::string logLevel = "debug";
        std::uint16_t port = 8080;
    };

    Config() = delete;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    static bool load(const std::string& path, ServerConfig& out);

    static std::string findDefaultConfigPath();
};