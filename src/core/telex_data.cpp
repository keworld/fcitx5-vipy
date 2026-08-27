//
// Created by keworld on 8/26/26.
//
#include "vicplex/telex_data.hpp"
#include <algorithm>
#include <cstring>


namespace vicplex {

constexpr char32_t TelexData::kVowelTable[12][6] = {
    {0x0061, 0x00E0, 0x00E1, 0x1EA3, 0x00E3, 0x1EA1},
    {0x0103, 0x1EB1, 0x1EAF, 0x1EB3, 0x1EB5, 0x1EB7},
    {0x00E2, 0x1EA7, 0x1EA5, 0x1EA9, 0x1EAB, 0x1EAD},
    {0x0065, 0x00E8, 0x00E9, 0x1EBB, 0x1EBD, 0x1EB9},
    {0x00EA, 0x1EC1, 0x1EBF, 0x1EC3, 0x1EC5, 0x1EC7},
    {0x0069, 0x00EC, 0x00ED, 0x1EC9, 0x0129, 0x1ECB},
    {0x006F, 0x00F2, 0x00F3, 0x1ECF, 0x00F5, 0x1ECD},
    {0x00F4, 0x1ED3, 0x1ED1, 0x1ED5, 0x1ED7, 0x1ED9},
    {0x01A1, 0x1EDD, 0x1EDB, 0x1EDF, 0x1EE1, 0x1EE3},
    {0x0075, 0x00F9, 0x00FA, 0x1EE7, 0x0169, 0x1EE5},
    {0x01B0, 0x1EEB, 0x1EE9, 0x1EED, 0x1EEF, 0x1EF1},
    {0x0079, 0x1EF3, 0x00FD, 0x1EF7, 0x1EF9, 0x1EF5},
};

constexpr TelexData::ComposeRule TelexData::kComposeRules[] = {
    {'a', 'a', 0x00E2}, {'a', 'w', 0x0103}, {'e', 'e', 0x00EA},
    {'o', 'o', 0x00F4}, {'o', 'w', 0x01A1}, {'u', 'w', 0x01B0},
    {'d', 'd', 0x0111},
};

constexpr TelexData::Ending TelexData::kValidEndings[] = {
    {"",   0},
    {"ch", 2}, {"ng", 2}, {"nh", 2},
    {"m",  1}, {"n", 1}, {"p", 1}, {"t", 1}, {"c", 1},
    {"o",  1}, {"u", 1}, {"i", 1}, {"y", 1},
    {"ao", 2}, {"ai", 2}, {"au", 2}, {"ay", 2},
    {"ua", 2}, {"ui", 2}, {"uy", 2}, {"oi", 2}, {"oe", 2},
};

const int TelexData::kTonePriorityRows[] = {8, 4, 7, 2, 1, 10};

const char *const TelexData::kToneSecondTriple[] = {
    "oai", "oay", "uay", "oeo", "oao", "uyu"
};

const char *const TelexData::kToneSecondPair[] = {
    "ie", "ia", "ua", "uo", "uy", "oa", "oe"
};

bool TelexData::vowelInfo(char32_t cp, int8_t &row, int8_t &tone) noexcept {
    const char32_t low = toLower(cp);
    for (int r = 0; r < kNumVowels; ++r) {
        for (int t = 0; t < kNumTones; ++t) {
            if (kVowelTable[r][t] == low) {
                row = static_cast<int8_t>(r);
                tone = static_cast<int8_t>(t);
                return true;
            }
        }
    }
    row = -1;
    tone = 0;
    return false;
}

int TelexData::vowelRow(char32_t cp) {
    int8_t r, t;
    return vowelInfo(cp, r, t) ? r : -1;
}

char32_t TelexData::vowelForm(int row, int tone) {
    return kVowelTable[row][tone];
}

const TelexData::ComposeRule *TelexData::findCompose(char second) {
    for (const auto &r : kComposeRules) {
        if (r.second == second) return &r;
    }
    return nullptr;
}

const TelexData::ComposeRule *TelexData::findComposePair(char a, char b) {
    for (const auto &r : kComposeRules) {
        if (r.first == a && r.second == b) return &r;
    }
    return nullptr;
}

bool TelexData::isValidEnding(const char *s, size_t len) noexcept {
    return std::ranges::any_of(kValidEndings, [s, len](const auto &e) {
        return e.len == len && std::memcmp(e.text, s, len) == 0;
    });
}

bool TelexData::toneOnSecondOfTriple(const char *s, size_t len) {
    return std::ranges::any_of(kToneSecondTriple, [s, len](const auto *t) {
        return std::strlen(t) == len && std::memcmp(t, s, len) == 0;
    });
}

bool TelexData::toneOnSecondOfPair(const char *s, size_t len) {
    return std::ranges::any_of(kToneSecondPair, [s, len](const auto *t) {
        return std::strlen(t) == len && std::memcmp(t, s, len) == 0;
    });
}

bool TelexData::skipsFirstVowel(char c0, char c1) {
    return (c0 == 'q' && c1 == 'u') || (c0 == 'g' && c1 == 'i');
}

char32_t TelexData::toUpper(char32_t cp) {
    if (cp >= 'a' && cp <= 'z') return cp - 0x20;
    if (cp >= 0x00E0 && cp <= 0x00FE && cp != 0x00F7) return cp - 0x20;
    if (cp == 0x0103 || cp == 0x0111) return cp - 1;
    if (cp == 0x01A1 || cp == 0x01B0) return cp - 1;
    if (cp >= 0x1EA1 && cp <= 0x1EF9 && (cp & 1)) return cp - 1;
    return cp;
}

char32_t TelexData::toLower(char32_t cp) {
    if (cp >= 'A' && cp <= 'Z') return cp + 0x20;
    if (cp >= 0x00C0 && cp <= 0x00DE && cp != 0x00D7) return cp + 0x20;
    if (cp == 0x0102 || cp == 0x0110) return cp + 1;
    if (cp == 0x01A0 || cp == 0x01AF) return cp + 1;
    if (cp >= 0x1EA0 && cp <= 0x1EF8 && !(cp & 1)) return cp + 1;
    return cp;
}

bool TelexData::isUpper(char32_t cp) { return toLower(cp) != cp; }

int TelexData::toneFromKey(char c) {
    switch (c) {
        case 'f': return 1;
        case 's': return 2;
        case 'r': return 3;
        case 'x': return 4;
        case 'j': return 5;
        default:  return 0;
    }
}

} // namespace vicplex