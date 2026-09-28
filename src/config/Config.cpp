#include "Config.h"

#include <cctype>
#include <cstdlib>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#endif

static std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

static std::string toLower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

std::string Config::findDefaultConfigPath() {
#ifdef _WIN32
    char buf[MAX_PATH] = {0};
    const DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
        std::string exePath(buf, n);
        const auto pos = exePath.find_last_of("\\/");
        if (pos != std::string::npos) {
            return exePath.substr(0, pos) + "\\config.ini";
        }
    }
    return "config.ini";
#else
    if (std::ifstream("/app/config/config.ini").good()) {
        return "/app/config/config.ini";
    }
    if (std::ifstream("config.ini").good()) {
        return "config.ini";
    }
    return "config.ini";
#endif
}

bool Config::load(const std::string& path, ServerConfig& out) {
    std::ifstream in(path);
    if (!in.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();

        const std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') continue;

        const auto eq = trimmed.find('=');
        if (eq == std::string::npos) continue;

        const std::string key = toLower(trim(trimmed.substr(0, eq)));
        const std::string value = trim(trimmed.substr(eq + 1));

        if (key == "log_level") {
            out.logLevel = toLower(value);
        } else if (key == "port") {
            try {
                const int p = std::stoi(value);
                if (p > 0 && p < 65536) {
                    out.port = static_cast<std::uint16_t>(p);
                }
            } catch (...) {

            }
        }
    }
    return true;
}