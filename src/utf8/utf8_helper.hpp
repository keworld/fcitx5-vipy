#ifndef VICPLEX_UTF8_HELPER_HPP
#define VICPLEX_UTF8_HELPER_HPP

#include <cstddef>
#include <string>

namespace vipy::utf8 {

inline void encode(char32_t cp, std::string &out) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

inline void encodeAll(const char32_t *cps, size_t len, std::string &out) {
    out.clear();
    out.reserve(len * 3);
    for (size_t i = 0; i < len; ++i) {
        encode(cps[i], out);
    }
}

} // namespace vipy::utf8

#endif // VICPLEX_UTF8_HELPER_HPP
