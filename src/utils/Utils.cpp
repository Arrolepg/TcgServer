#include "Utils.h"

#include <random>

#ifdef _WIN32
#include <windows.h>
#endif

namespace Utils {

    std::string ansiToUtf8(const std::string& ansi) {
#ifdef _WIN32
        int wlen = MultiByteToWideChar(CP_ACP, 0, ansi.c_str(), -1, nullptr, 0);
        std::wstring wstr(wlen, L'\0');
        MultiByteToWideChar(CP_ACP, 0, ansi.c_str(), -1, &wstr[0], wlen);

        int utf8len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1,
                                          nullptr, 0, nullptr, nullptr);
        std::string utf8(utf8len, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1,
                            &utf8[0], utf8len, nullptr, nullptr);

        if (!utf8.empty() && utf8.back() == '\0') utf8.pop_back();
        return utf8;
#else
        return ansi;
#endif
    }

    std::string generateRoomId() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(1000, 9999);
        return "ROOM-" + std::to_string(dis(gen));
    }
}