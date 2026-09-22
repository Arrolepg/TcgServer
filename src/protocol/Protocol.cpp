#include "Protocol.h"

namespace Protocol {

    std::string readString(const std::vector<char>& data, std::size_t& pos) {
        std::string result;
        while (pos < data.size() && data[pos] != '\0') {
            result += data[pos++];
        }
        if (pos < data.size() && data[pos] == '\0') {
            ++pos;
        }
        return result;
    }

    std::vector<char> buildResponse(Opcode code, const std::string& payload) {
        std::vector<char> response;
        response.reserve(payload.size() + 2);
        response.push_back(static_cast<char>(code));
        response.insert(response.end(), payload.begin(), payload.end());
        response.push_back('\0');
        return response;
    }

}