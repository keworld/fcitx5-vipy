#include "vicplex/vni_transformer.hpp"

#include "vicplex/char_util.hpp"
#include "vicplex/telex_data.hpp"
#include "vicplex/telex_transformer.hpp"
#include "vicplex/vni_data.hpp"

#include <string>

namespace vicplex {

namespace {

char baseLetter(char32_t cp) {
    const char32_t low = TelexData::toLower(cp);
    int8_t row, tone;
    if (!TelexData::vowelInfo(low, row, tone)) return CharUtil::asciiLower(low);
    switch (TelexData::vowelForm(row, 0)) {
        case 'a': case 0x0103: case 0x00E2: return 'a';
        case 'e': case 0x00EA: return 'e';
        case 'i': return 'i';
        case 'o': case 0x00F4: case 0x01A1: return 'o';
        case 'u': case 0x01B0: return 'u';
        case 'y': return 'y';
        default: return '\0';
    }
}

void setVowelForm(CharBuffer &buf, size_t index, int row) {
    int8_t oldRow, tone;
    TelexData::vowelInfo(buf[index], oldRow, tone);
    const bool upper = TelexData::isUpper(buf[index]);
    char32_t cp = TelexData::vowelForm(row, tone);
    buf[index] = upper ? TelexData::toUpper(cp) : cp;
}

int findToneIndex(const CharBuffer &buf) {
    TelexTransformer::VowelPos vowels[kMaxWordLength];
    int n = 0;
    for (size_t i = 0; i < buf.size(); ++i) {
        int8_t row, tone;
        bool upper;
        TelexData::vowelInfo(buf[i], row, tone);
        if (row >= 0) vowels[n++] = {static_cast<uint8_t>(i), row, tone, TelexData::isUpper(buf[i])};
    }
    if (n == 0) return -1;

    for (int row : TelexData::kTonePriorityRows)
        for (int i = 0; i < n; ++i)
            if (vowels[i].row == row) return vowels[i].index;

    size_t start = 0;
    if (n > 1 && buf.size() >= 2 &&
        TelexData::skipsFirstVowel(CharUtil::asciiLower(buf[0]),
                                   CharUtil::asciiLower(buf[1]))) {
        start = 1;
    }
    auto pairName = [&](size_t at, size_t count) {
        char result[4] = {};
        for (size_t i = 0; i < count; ++i) result[i] = baseLetter(buf[vowels[at + i].index]);
        return std::string(result, count);
    };
    if (n - start >= 3 && TelexData::toneOnSecondOfTriple(pairName(start, 3).c_str(), 3))
        return vowels[start + 1].index;
    if (n - start >= 2 && TelexData::toneOnSecondOfPair(pairName(start, 2).c_str(), 2)) {
        const size_t after = vowels[start + 1].index + 1;
        return after < buf.size() ? vowels[start + 1].index : vowels[start].index;
    }
    return vowels[start].index;
}

bool render(CharBuffer &buf) {
    CharBuffer rendered;
    int tone = 0;
    for (size_t i = 0; i < buf.vniRawSize(); ++i) {
        const char c = buf.vniRawAt(i);
        if (VniData::toneFromKey(c) != 0) tone = VniData::toneFromKey(c);
        else if (CharUtil::isAsciiLetter(static_cast<unsigned char>(c))) rendered.push(static_cast<unsigned char>(c));
    }
    if (rendered.empty()) return false;

    for (size_t i = 0; i < buf.vniRawSize(); ++i) {
        const char key = buf.vniRawAt(i);
        if (key == '9') {
            for (size_t j = 0; j < rendered.size(); ++j)
                if (CharUtil::asciiLower(rendered[j]) == 'd') {
                    rendered[j] = TelexData::isUpper(rendered[j]) ? 0x0110 : 0x0111;
                    break;
                }
        } else if (key == '6' || key == '8') {
            const char wanted = key == '8' ? 'a' : '\0';
            for (size_t j = 0; j < rendered.size(); ++j) {
                const char b = baseLetter(rendered[j]);
                if (b == wanted || (key == '6' && (b == 'a' || b == 'e' || b == 'o'))) {
                    const int row = key == '8' ? 1 : (b == 'a' ? 2 : (b == 'e' ? 4 : 7));
                    setVowelForm(rendered, j, row);
                    break;
                }
            }
        } else if (key == '7') {
            for (size_t j = 0; j < rendered.size(); ++j) {
                if (baseLetter(rendered[j]) != 'u') continue;
                size_t next = j + 1;
                if (next < rendered.size() && baseLetter(rendered[next]) == 'o') {
                    const bool hasFollowing = next + 1 < rendered.size();
                    if (hasFollowing) setVowelForm(rendered, j, 10);
                    setVowelForm(rendered, next, 8);
                    break;
                }
                setVowelForm(rendered, j, 10);
                break;
            }
            for (size_t j = 0; j < rendered.size(); ++j)
                if (baseLetter(rendered[j]) == 'o' && (j == 0 || baseLetter(rendered[j - 1]) != 'u'))
                    setVowelForm(rendered, j, 8);
        }
    }

    if (tone != 0) {
        const int target = findToneIndex(rendered);
        if (target < 0 || !TelexTransformer::isValidSyllableStructure(rendered)) return false;
        for (size_t i = 0; i < rendered.size(); ++i) {
            int8_t row, oldTone;
            TelexData::vowelInfo(rendered[i], row, oldTone);
            if (row >= 0 && oldTone != 0) {
                const bool upper = TelexData::isUpper(rendered[i]);
                rendered[i] = upper ? TelexData::toUpper(TelexData::vowelForm(row, 0))
                                    : TelexData::vowelForm(row, 0);
            }
        }
        int8_t row, oldTone;
        TelexData::vowelInfo(rendered[static_cast<size_t>(target)], row, oldTone);
        char32_t cp = TelexData::vowelForm(row, tone);
        rendered[static_cast<size_t>(target)] =
            TelexData::isUpper(rendered[static_cast<size_t>(target)]) ? TelexData::toUpper(cp) : cp;
    }
    buf.clearContent();
    for (size_t i = 0; i < rendered.size(); ++i) buf.push(rendered[i]);
    return true;
}

bool hasVowelShape(const CharBuffer &buf, char key) {
    for (size_t i = 0; i < buf.size(); ++i) {
        const char base = baseLetter(buf[i]);
        if ((key == '6' && (base == 'a' || base == 'e' || base == 'o')) ||
            (key == '7' && (base == 'u' || base == 'o')) ||
            (key == '8' && base == 'a')) {
            return true;
        }
    }
    return false;
}

bool digitApplies(const CharBuffer &buf, char key) {
    if (buf.empty()) return false;
    if (key == '9') {
        for (size_t i = 0; i < buf.size(); ++i)
            if (CharUtil::asciiLower(buf[i]) == 'd') return true;
        return false;
    }
    if (TelexTransformer::countVowelClusters(buf) != 1) return false;
    if (VniData::toneFromKey(key) != 0)
        return TelexTransformer::hasVowelChar(buf);
    return hasVowelShape(buf, key);
}

} // namespace

bool VniTransformer::processKey(CharBuffer &buf, char key) {
    const bool accepted = CharUtil::isAsciiLetter(static_cast<unsigned char>(key)) ||
        (key >= '1' && key <= '9');
    if (!accepted || buf.vniRawSize() >= kMaxWordLength) return false;
    if (key >= '1' && key <= '9' && !digitApplies(buf, key)) return false;

    if (VniData::toneFromKey(key) != 0) {
        for (size_t i = 0; i < buf.vniRawSize(); ++i) {
            if (buf.vniRawAt(i) == key) {
                buf.eraseVniRaw(i);
                return render(buf);
            }
        }
    }
    buf.pushVniRaw(key);
    if (!render(buf)) {
        buf.popVniRaw();
        return false;
    }
    return true;
}

bool VniTransformer::tryShapeKey(CharBuffer &buf, char key) {
    for (const auto &rule : VniData::kShapeRules) {
        if (rule.key != key) continue;
        for (size_t i = buf.size(); i-- > 0;) {
            const char32_t low = TelexData::toLower(buf[i]);
            if (low == static_cast<unsigned char>(rule.base)) {
                int8_t row, tone;
                bool upper;
                TelexData::vowelInfo(low, row, tone);
                upper = TelexData::isUpper(buf[i]);
                if (rule.base == 'd') {
                    buf[i] = upper ? 0x0110 : 0x0111;
                } else {
                    char32_t result = TelexData::vowelForm(
                        TelexData::vowelRow(rule.result), tone);
                    buf[i] = upper ? TelexData::toUpper(result) : result;
                }
                return true;
            }

            int8_t row, tone;
            TelexData::vowelInfo(low, row, tone);
            if (row >= 0 && TelexData::vowelForm(row, 0) == rule.base) {
                char32_t result = TelexData::vowelForm(
                    TelexData::vowelRow(rule.result), tone);
                buf[i] = TelexData::isUpper(buf[i]) ? TelexData::toUpper(result) : result;
                return true;
            }
            if (row >= 0 && TelexData::vowelForm(row, 0) == rule.result) return true;
        }
    }
    return false;
}

bool VniTransformer::tryToneKey(CharBuffer &buf, char key) {
    const int targetTone = VniData::toneFromKey(key);
    size_t indices[kMaxWordLength];
    int8_t rows[kMaxWordLength], tones[kMaxWordLength];
    bool uppers[kMaxWordLength];
    const int count = collectVowels(buf, indices, rows, tones, uppers);
    if (count == 0) return false;

    for (int i = 0; i < count; ++i) {
        if (tones[i] == targetTone) {
            stripTones(buf, indices, rows, tones, uppers, count);
            return true;
        }
    }
    if (!TelexTransformer::isValidSyllableStructure(buf)) return false;

    stripTones(buf, indices, rows, tones, uppers, count);
    const int target = findToneIndex(buf, indices, rows, count);
    if (target < 0) return false;
    applyTone(buf, indices[target], targetTone);
    return true;
}

int VniTransformer::collectVowels(const CharBuffer &buf, size_t *indices,
                                  int8_t *rows, int8_t *tones, bool *uppers) {
    int count = 0;
    for (size_t i = 0; i < buf.size(); ++i) {
        TelexData::vowelInfo(buf[i], rows[count], tones[count]);
        if (rows[count] < 0) continue;
        indices[count] = i;
        uppers[count] = TelexData::isUpper(buf[i]);
        ++count;
    }
    return count;
}

int VniTransformer::findToneIndex(const CharBuffer &buf, const size_t *indices,
                                  const int8_t *rows, int count) {
    for (int priority : TelexData::kTonePriorityRows) {
        for (int i = 0; i < count; ++i) {
            if (rows[i] == priority) return i;
        }
    }

    size_t start = 0;
    if (count > 1 && buf.size() >= 2 &&
        TelexData::skipsFirstVowel(CharUtil::asciiLower(buf[0]),
                                   CharUtil::asciiLower(buf[1]))) {
        start = 1;
    }
    if (count >= 2) {
        auto baseLetter = [](int8_t row) {
            switch (TelexData::vowelForm(row, 0)) {
                case 'a': case 0x0103: case 0x00E2: return 'a';
                case 'e': case 0x00EA: return 'e';
                case 'i': return 'i';
                case 'o': case 0x00F4: case 0x01A1: return 'o';
                case 'u': case 0x01B0: return 'u';
                case 'y': return 'y';
                default: return '\0';
            }
        };
        char pair[2] = {
            baseLetter(rows[start]),
            baseLetter(rows[start + 1]),
        };
        if (TelexData::toneOnSecondOfPair(pair, 2)) return static_cast<int>(start + 1);
    }
    return static_cast<int>(start);
}

void VniTransformer::stripTones(CharBuffer &buf, const size_t *indices,
                                const int8_t *rows, const int8_t *tones,
                                const bool *uppers, int count) {
    for (int i = 0; i < count; ++i) {
        if (tones[i] == 0) continue;
        char32_t base = TelexData::vowelForm(rows[i], 0);
        buf[indices[i]] = uppers[i] ? TelexData::toUpper(base) : base;
    }
}

void VniTransformer::applyTone(CharBuffer &buf, size_t index, int tone) {
    int8_t row, currentTone;
    TelexData::vowelInfo(buf[index], row, currentTone);
    if (row < 0) return;
    char32_t result = TelexData::vowelForm(row, tone);
    if (TelexData::isUpper(buf[index])) result = TelexData::toUpper(result);
    buf[index] = result;
}

} // namespace vicplex
