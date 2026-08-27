//
// Created by keworld on 8/26/26.
//
#include "vicplex/telex_transformer.hpp"
#include "vicplex/char_util.hpp"
#include "vicplex/utf8_helper.hpp"

#if defined(_MSC_VER)
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#else
#endif

namespace vicplex {

TelexTransformer::TelexTransformer(SyllableDict &dict) : dict_(dict) {}

bool TelexTransformer::processKey(CharBuffer &buf, char key) {
    const char kLow = (key >= 'A' && key <= 'Z') ? static_cast<char>(key - 'A' + 'a') : key;
    const bool keyUpper = (key >= 'A' && key <= 'Z');

    if (tryDecompose(buf, kLow, keyUpper)) return true;
    if (kLow == 'w' && tryWRule(buf, keyUpper)) return true;
    if (tryDoubleCompose(buf, kLow)) return true;
    if (tryToneKey(buf, kLow)) return true;

    appendNormalChar(buf, key);
    return true;
}

bool TelexTransformer::hasVowelChar(const CharBuffer &buf) {
    for (size_t i = 0; i < buf.size(); ++i) {
        if (CharUtil::isVowel(buf[i])) return true;
    }
    return false;
}

bool TelexTransformer::isLikelyVietnamese(const CharBuffer &buf) {
    if (!hasVietnameseMarks(buf)) return false;
    return countVowelClusters(buf) == 1 && isValidSyllableStructure(buf);
}

bool TelexTransformer::inDictionary(const CharBuffer &buf) {
    std::string low;
    low.reserve(buf.size());
    for (size_t i = 0; i < buf.size(); ++i) {
        utf8::encode(TelexData::toLower(buf[i]), low);
    }
    return vicplex::SyllableDict::contains(low);
}

void TelexTransformer::plainInfo(char32_t cp, int8_t &row, int8_t &tone, bool &upper) noexcept {
    upper = TelexData::isUpper(cp);
    TelexData::vowelInfo(cp, row, tone);
}

bool TelexTransformer::tryDecompose(CharBuffer &buf, char kLow, bool) {
    if (buf.empty()) return false;

    const char32_t lastLow = TelexData::toLower(buf.back());
    const auto *rule = TelexData::findCompose(kLow);
    if (!rule || rule->result != lastLow) return false;

    const bool upper = TelexData::isUpper(buf.back());
    buf.pop();

    char32_t firstCp = static_cast<unsigned char>(rule->first);
    if (upper) firstCp = firstCp - 0x20;
    buf.push(firstCp);
    buf.push(static_cast<unsigned char>(kLow));
    return true;
}

bool TelexTransformer::tryWRule(CharBuffer &buf, bool keyUpper) {
    size_t cStart = 0, cEnd = 0;
    if (getLastVowelCluster(buf, cStart, cEnd)) {
        if (transformWClusterInPlace(buf, cStart, cEnd)) {
            relocateTone(buf);
            return true;
        }
    }

    const bool upper = !buf.empty() ? TelexData::isUpper(buf.back()) : keyUpper;
    buf.push(upper ? 0x01AF : 0x01B0);
    return true;
}

bool TelexTransformer::transformWClusterInPlace(CharBuffer &buf, size_t cStart, size_t cEnd) {
    for (size_t i = cStart; i + 1 <= cEnd; ++i) {
        int8_t aRow, aTone, bRow, bTone;
        bool aUpper, bUpper;
        plainInfo(buf[i], aRow, aTone, aUpper);
        plainInfo(buf[i + 1], bRow, bTone, bUpper);
        const bool isU = aRow >= 0 && TelexData::vowelForm(aRow, 0) == 'u';
        const bool isO = bRow == 6 || bRow == 7;
        if (isU && isO) {
            const bool hasFollowingMaterial = cEnd > i + 1 || cEnd + 1 < buf.size();
            if (hasFollowingMaterial) {
                char32_t u = TelexData::vowelForm(10, aTone);
                buf[i] = aUpper ? TelexData::toUpper(u) : u;
            }
            char32_t o = TelexData::vowelForm(8, bTone);
            buf[i + 1] = bUpper ? TelexData::toUpper(o) : o;
            return true;
        }
        if (aRow == 10 && bRow == 8) {
            return true;
        }
    }

    if (cEnd < cStart) return false;
    const bool upper = TelexData::isUpper(buf[cEnd]);

    int8_t row, tone;
    if (TelexData::vowelInfo(buf[cEnd], row, tone)) {
        int targetRow = -1;
        if (row == 0) targetRow = 1;       // a + w -> ă
        if (row == 6) targetRow = 8;       // o + w -> ơ
        if (row == 9) targetRow = 10;      // u + w -> ư
        if (targetRow >= 0) {
            char32_t result = TelexData::vowelForm(targetRow, tone);
            buf[cEnd] = upper ? TelexData::toUpper(result) : result;
            return true;
        }
    }
    return false;
}

bool TelexTransformer::tryDoubleCompose(CharBuffer &buf, char kLow) {
    const auto *rule = TelexData::findComposePair(kLow, kLow);
    if (!rule) return false;

    for (size_t ii = buf.size(); ii-- > 0;) {
        int8_t row, tone;
        bool upper;
        plainInfo(buf[ii], row, tone, upper);

        const char32_t low = TelexData::toLower(buf[ii]);
        const bool isBaseMatch = low == static_cast<char32_t>(kLow) ||
            (row >= 0 &&
             TelexData::vowelForm(row, 0) == static_cast<char32_t>(kLow));
        if (!isBaseMatch) continue;

        char32_t composed = rule->result;
        if (upper) composed = TelexData::toUpper(composed);
        if (tone > 0 && TelexData::vowelRow(composed) >= 0) {
            composed = TelexData::vowelForm(TelexData::vowelRow(composed), tone);
            if (upper) composed = TelexData::toUpper(composed);
        }
        buf[ii] = composed;
        relocateTone(buf);
        return true;
    }
    return false;
}

bool TelexTransformer::tryToneKey(CharBuffer &buf, char kLow) {
    const int targetTone = TelexData::toneFromKey(kLow);
    if (targetTone == 0) return false;

    VowelPos vowels[kMaxWordLength];
    int nV = collectVowels(buf, vowels);
    if (nV == 0) return false;

    int currentTone = 0;
    for (int i = 0; i < nV; ++i) {
        if (vowels[i].tone > 0) { currentTone = vowels[i].tone; break; }
    }

    if (currentTone == targetTone) {
        stripTones(buf, vowels, nV);
        return true;
    }

    if (!isValidSyllableStructure(buf)) return false;

    stripTones(buf, vowels, nV);
    const int bestIdx = findBestVowelIdx(vowels, nV, buf);
    if (bestIdx < 0) return false;

    applyToneAt(buf, bestIdx, targetTone);
    return true;
}

void TelexTransformer::promoteUoBeforeNewChar(CharBuffer &buf) {
    const size_t n = buf.size();
    if (n < 2) return;
    int8_t row, tone;
    TelexData::vowelInfo(buf[n - 1], row, tone);
    if (TelexData::toLower(buf[n - 2]) == 'u' && row == 8) {
        const bool upper = TelexData::isUpper(buf[n - 2]);
        buf[n - 2] = upper ? 0x01AF : 0x01B0;
    }
}

void TelexTransformer::appendNormalChar(CharBuffer &buf, char key) {
    promoteUoBeforeNewChar(buf);
    buf.push(static_cast<unsigned char>(key));
    relocateTone(buf);
}

int TelexTransformer::collectVowels(const CharBuffer &buf, VowelPos *out) {
    int n = 0;
    for (size_t i = 0; i < buf.size(); ++i) {
        int8_t row, tone;
        bool upper;
        plainInfo(buf[i], row, tone, upper);
        if (row >= 0) {
            out[n++] = VowelPos{static_cast<uint8_t>(i), row, tone, upper};
        }
    }
    return n;
}

bool TelexTransformer::getLastVowelCluster(const CharBuffer &buf, size_t &start, size_t &end) {
    ssize_t lastVowel = -1;
    for (ssize_t i = static_cast<ssize_t>(buf.size()) - 1; i >= 0; --i) {
        if (CharUtil::isVowel(buf[static_cast<size_t>(i)])) {
            lastVowel = i;
            break;
        }
    }
    if (lastVowel < 0) return false;
    end = static_cast<size_t>(lastVowel);
    start = end;

    while (start > 0 && CharUtil::isVowel(buf[start - 1])) {
        --start;
    }
    return true;
}

int TelexTransformer::findBestVowelIdx(const VowelPos *vowels, int n, const CharBuffer &buf) {
    if (n == 0) return -1;
    if (n == 1) return vowels[0].index;

    for (int pr : TelexData::kTonePriorityRows) {
        for (int i = 0; i < n; ++i) {
            if (vowels[i].row == pr) return vowels[i].index;
        }
    }

    size_t startIdx = 0;
    if (n > 1 && buf.size() >= 2 &&
        TelexData::skipsFirstVowel(CharUtil::asciiLower(buf[0]), CharUtil::asciiLower(buf[1]))) {
        startIdx = 1;
    }

    const size_t remaining = static_cast<size_t>(n) - startIdx;

    if (remaining >= 3) {
        char v3[3] = {
            static_cast<char>('a' + baseLetter(vowels, startIdx, buf)),
            static_cast<char>('a' + baseLetter(vowels, startIdx + 1, buf)),
            static_cast<char>('a' + baseLetter(vowels, startIdx + 2, buf)),
        };
        if (TelexData::toneOnSecondOfTriple(v3, 3)) {
            return vowels[startIdx + 1].index;
        }
    }

    if (remaining >= 2) {
        char v2[2] = {
            static_cast<char>('a' + baseLetter(vowels, startIdx, buf)),
            static_cast<char>('a' + baseLetter(vowels, startIdx + 1, buf)),
        };
        if (TelexData::toneOnSecondOfPair(v2, 2)) {
            const size_t lastIdx = vowels[n - 1].index;
            char ending[kMaxWordLength] = {};
            size_t elen = 0;
            for (size_t i = lastIdx + 1; i < buf.size() && elen < kMaxWordLength; ++i) {
                ending[elen++] = CharUtil::asciiLower(buf[i]);
            }
            const bool hasEnding = elen > 0 && TelexData::isValidEnding(ending, elen);
            return hasEnding ? vowels[startIdx + 1].index : vowels[startIdx].index;
        }
    }

    return vowels[startIdx].index;
}

uint8_t TelexTransformer::baseLetter(const VowelPos *vowels, size_t vi, const CharBuffer &buf) {
    const char32_t base = TelexData::vowelForm(vowels[vi].row, 0);
    switch (base) {
        case 0x0061: case 0x0103: case 0x00E2: return 'a' - 'a';
        case 0x0065: case 0x00EA:               return 'e' - 'a';
        case 0x0069:                            return 'i' - 'a';
        case 0x006F: case 0x00F4: case 0x01A1:  return 'o' - 'a';
        case 0x0075: case 0x01B0:               return 'u' - 'a';
        case 0x0079:                            return 'y' - 'a';
        default: break;
    }
    return static_cast<uint8_t>(CharUtil::asciiLower(buf[vowels[vi].index]) - 'a');
}

void TelexTransformer::stripTones(CharBuffer &buf, const VowelPos *vowels, int n) {
    for (int i = 0; i < n; ++i) {
        if (vowels[i].tone <= 0) continue;
        char32_t base = TelexData::vowelForm(vowels[i].row, 0);
        if (vowels[i].upper) base = TelexData::toUpper(base);
        buf[vowels[i].index] = base;
    }
}

void TelexTransformer::applyToneAt(CharBuffer &buf, int index, int tone) {
    int8_t row = TelexData::vowelRow(TelexData::toLower(buf[index]));
    if (row < 0) return;
    const bool upper = TelexData::isUpper(buf[index]);
    char32_t cp = TelexData::vowelForm(row, tone);
    if (upper) cp = TelexData::toUpper(cp);
    buf[index] = cp;
}

void TelexTransformer::relocateTone(CharBuffer &buf) {
    VowelPos vowels[kMaxWordLength];
    const int n = collectVowels(buf, vowels);
    int currentTone = 0;
    for (int i = 0; i < n; ++i) {
        if (vowels[i].tone > 0) { currentTone = vowels[i].tone; break; }
    }
    if (currentTone == 0) return;

    stripTones(buf, vowels, n);
    const int bestIdx = findBestVowelIdx(vowels, n, buf);
    if (bestIdx < 0) return;

    applyToneAt(buf, bestIdx, currentTone);
}

bool TelexTransformer::isValidSyllableStructure(const CharBuffer &buf) {
    if (buf.empty()) return false;

    ssize_t lastVowelIdx = -1;
    for (ssize_t i = static_cast<ssize_t>(buf.size()) - 1; i >= 0; --i) {
        if (CharUtil::isVowel(buf[static_cast<size_t>(i)])) {
            lastVowelIdx = i;
            break;
        }
    }
    if (lastVowelIdx < 0) return false;

    char ending[kMaxWordLength];
    size_t elen = 0;
    for (size_t i = static_cast<size_t>(lastVowelIdx) + 1; i < buf.size(); ++i) {
        ending[elen++] = CharUtil::asciiLower(buf[i]);
    }
    return TelexData::isValidEnding(ending, elen);
}

size_t TelexTransformer::countVowelClusters(const CharBuffer &buf) {
    size_t count = 0;
    bool inCluster = false;
    for (size_t i = 0; i < buf.size(); ++i) {
        const bool v = CharUtil::isVowel(buf[i]);
        if (v && !inCluster) ++count;
        inCluster = v;
    }
    return count;
}

bool TelexTransformer::hasVietnameseMarks(const CharBuffer &buf) {
    for (size_t i = 0; i < buf.size(); ++i) {
        if (!CharUtil::isAsciiLetter(buf[i])) return true;
    }
    return false;
}

} // namespace vicplex