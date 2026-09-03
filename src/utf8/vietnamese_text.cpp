#include "vietnamese_text.hpp"

#include "utf8_helper.hpp"

namespace vipy::utf8 {

std::string lowercaseVietnamese(const std::string &value) {
    std::string result;
    result.reserve(value.size());
    for (size_t i = 0; i < value.size();) {
        const auto first = static_cast<unsigned char>(value[i]);
        if (first < 0x80) {
            result.push_back(first >= 'A' && first <= 'Z'
                                ? static_cast<char>(first - 'A' + 'a')
                                : static_cast<char>(first));
            ++i;
            continue;
        }

        const size_t length = first < 0xE0 ? 2 : first < 0xF0 ? 3 : 4;
        if (i + length > value.size()) {
            result.append(value, i, std::string::npos);
            break;
        }
        char32_t codepoint =
            first & (length == 2 ? 0x1F : length == 3 ? 0x0F : 0x07);
        bool valid = true;
        for (size_t j = 1; j < length; ++j) {
            const auto byte = static_cast<unsigned char>(value[i + j]);
            if ((byte & 0xC0) != 0x80) {
                valid = false;
                break;
            }
            codepoint = (codepoint << 6) | (byte & 0x3F);
        }
        if (!valid) {
            result.push_back(value[i++]);
            continue;
        }
        if (codepoint == 0x0102 || codepoint == 0x00C2 ||
            codepoint == 0x00CA || codepoint == 0x00D4 ||
            codepoint == 0x0110 || codepoint == 0x01A0 ||
            codepoint == 0x01AF ||
            (codepoint >= 0x1EA0 && codepoint <= 0x1EF8 &&
             (codepoint & 1) == 0)) {
            ++codepoint;
        }
        encode(codepoint, result);
        i += length;
    }
    return result;
}

} // namespace vipy::utf8
