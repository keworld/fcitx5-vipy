#include "vicplex/telex_transformer.hpp"
#include "vicplex/utf8_helper.hpp"
#include "vicplex/vni_transformer.hpp"

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace vicplex;

std::string transform(const char *input) {
    vicplex::CharBuffer buf;
    for (const char *p = input; *p != '\0'; ++p) {
        vicplex::TelexTransformer::processKey(buf, *p);
    }

    std::string result;
    vicplex::utf8::encodeAll(buf.data(), buf.size(), result);
    return result;
}

std::string transformVni(const char *input) {
    vicplex::CharBuffer buf;
    for (const char *p = input; *p != '\0'; ++p) {
        vicplex::VniTransformer::processKey(buf, *p);
    }

    std::string result;
    vicplex::utf8::encodeAll(buf.data(), buf.size(), result);
    return result;
}

std::string commitOutput(const char *input) {
    vicplex::CharBuffer current;
    vicplex::CharBuffer raw;
    for (const char *p = input; *p != '\0'; ++p) {
        vicplex::TelexTransformer::processKey(current, *p);
        raw.push(static_cast<unsigned char>(*p));
    }

    const vicplex::CharBuffer &src =
        !current.empty() &&
        vicplex::TelexTransformer::isLikelyVietnamese(current) &&
        vicplex::TelexTransformer::inDictionary(current) ? current : raw;

    std::string result;
    vicplex::utf8::encodeAll(src.data(), src.size(), result);
    return result;
}

std::string commitOutputVni(const char *input) {
    vicplex::CharBuffer current;
    vicplex::CharBuffer raw;
    for (const char *p = input; *p != '\0'; ++p) {
        if (!vicplex::VniTransformer::processKey(current, *p)) {
            raw.push(static_cast<unsigned char>(*p));
            continue;
        }
        raw.push(static_cast<unsigned char>(*p));
        if (!vicplex::TelexTransformer::isLikelyVietnamese(current) &&
            vicplex::TelexTransformer::hasVowelChar(current)) {
            current.assignContentFrom(raw);
        }
    }

    const vicplex::CharBuffer &src =
        !current.empty() &&
        vicplex::TelexTransformer::isLikelyVietnamese(current) ? current : raw;

    std::string result;
    vicplex::utf8::encodeAll(src.data(), src.size(), result);
    return result;
}

bool expect(const char *input, const char *expected) {
    const std::string actual = transform(input);
    if (actual == expected) return true;
    std::cerr << input << " -> " << actual << ", expected " << expected << '\n';
    return false;
}

bool expectVni(const char *input, const char *expected) {
    const std::string actual = transformVni(input);
    if (actual == expected) return true;
    std::cerr << "VNI " << input << " -> " << actual << ", expected " << expected << '\n';
    return false;
}

bool expectVniConsumed(const char *input, char key) {
    vicplex::CharBuffer buf;
    for (const char *p = input; *p != '\0'; ++p) {
        if (!vicplex::VniTransformer::processKey(buf, *p)) {
            std::cerr << "VNI rejected " << input << '\n';
            return false;
        }
    }
    if (vicplex::VniTransformer::processKey(buf, key)) return true;
    std::cerr << "VNI did not consume " << input << key << '\n';
    return false;
}

bool expectVniLiteral(const char *input, char key) {
    vicplex::CharBuffer buf;
    for (const char *p = input; *p != '\0'; ++p)
        vicplex::VniTransformer::processKey(buf, *p);
    if (!vicplex::VniTransformer::processKey(buf, key)) return true;
    std::cerr << "VNI incorrectly consumed " << input << key << '\n';
    return false;
}

bool expectVniCommit(const char *input, char suffix, const char *expected) {
    const std::string actual = commitOutputVni(input) + suffix;
    if (actual == expected) return true;
    std::cerr << "VNI commit " << input << suffix << " -> " << actual
              << ", expected " << expected << '\n';
    return false;
}

std::vector<char32_t> decodeUtf8(std::string_view input) {
    std::vector<char32_t> result;
    for (size_t i = 0; i < input.size();) {
        const unsigned char c = static_cast<unsigned char>(input[i++]);
        if (c < 0x80) {
            result.push_back(c);
        } else if ((c & 0xe0) == 0xc0) {
            result.push_back((c & 0x1f) << 6 |
                             (static_cast<unsigned char>(input[i++]) & 0x3f));
        } else if ((c & 0xf0) == 0xe0) {
            result.push_back((c & 0x0f) << 12 |
                             (static_cast<unsigned char>(input[i++]) & 0x3f) << 6 |
                             (static_cast<unsigned char>(input[i++]) & 0x3f));
        } else {
            result.push_back((c & 0x07) << 18 |
                             (static_cast<unsigned char>(input[i++]) & 0x3f) << 12 |
                             (static_cast<unsigned char>(input[i++]) & 0x3f) << 6 |
                             (static_cast<unsigned char>(input[i++]) & 0x3f));
        }
    }
    return result;
}

char vowelBase(int row) {
    switch (TelexData::vowelForm(row, 0)) {
        case 'a': case 0x0103: case 0x00e2: return 'a';
        case 'e': case 0x00ea: return 'e';
        case 'i': return 'i';
        case 'o': case 0x00f4: case 0x01a1: return 'o';
        case 'u': case 0x01b0: return 'u';
        case 'y': return 'y';
        default: return '\0';
    }
}

std::string encodeWord(std::string_view word, bool vni) {
    std::string keys;
    int tone = 0;
    for (const char32_t cp : decodeUtf8(word)) {
        if (cp == 0x0111 || cp == 0x0110) {
            keys += cp == 0x0110 ? "D" : "d";
            if (vni) keys += '9';
            else keys += cp == 0x0110 ? "d" : "d";
            continue;
        }

        int8_t row, currentTone;
        if (!TelexData::vowelInfo(cp, row, currentTone)) {
            keys.push_back(static_cast<char>(cp));
            continue;
        }
        if (currentTone != 0) tone = currentTone;
        const char base = vowelBase(row);
        const bool upper = TelexData::isUpper(cp);
        keys.push_back(upper ? static_cast<char>(base - 'a' + 'A') : base);
        if (row == 1) keys += vni ? "8" : "w";
        if (row == 2) keys += vni ? "6" : "a";
        if (row == 4) keys += vni ? "6" : "e";
        if (row == 7) keys += vni ? "6" : "o";
        if (row == 8 || row == 10) keys += vni ? "7" : "w";
    }
    if (tone != 0) {
        if (vni) {
            static constexpr char vniTone[] = {0, '2', '1', '3', '4', '5'};
            keys += vniTone[tone];
        }
        else keys += "fsrxj"[tone - 1];
    }
    return keys;
}

bool hasVietnameseLetter(std::string_view word) {
    for (const char32_t cp : decodeUtf8(word))
        if (cp > 0x7f) return true;
    return false;
}

std::string transformCorpus(std::string_view paragraph, bool vni) {
    std::string output;
    std::string token;
    auto flushToken = [&] {
        if (token.empty()) return;
        if (!hasVietnameseLetter(token)) {
            output += token;
        } else {
            CharBuffer current;
            CharBuffer raw;
            const std::string keys = encodeWord(token, vni);
            for (const char key : keys) {
                const bool consumed = vni
                    ? VniTransformer::processKey(current, key)
                    : TelexTransformer::processKey(current, key);
                raw.push(static_cast<unsigned char>(key));
                if (vni && consumed &&
                    !TelexTransformer::isLikelyVietnamese(current) &&
                    TelexTransformer::hasVowelChar(current)) {
                    current.assignContentFrom(raw);
                }
            }
            const bool valid = !current.empty() &&
                TelexTransformer::isLikelyVietnamese(current) &&
                (vni || TelexTransformer::inDictionary(current));
            std::string committed;
            const CharBuffer &source = valid ? current : raw;
            utf8::encodeAll(source.data(), source.size(), committed);
            output += committed;
        }
        token.clear();
    };

    for (const char32_t cp : decodeUtf8(paragraph)) {
        const bool asciiWordChar =
            (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') ||
            (cp >= '0' && cp <= '9');
        int8_t row, tone;
        const bool vietnameseLetter = TelexData::vowelInfo(cp, row, tone) ||
            cp == 0x0111 || cp == 0x0110;
        if (asciiWordChar || vietnameseLetter) {
            utf8::encode(cp, token);
        } else {
            flushToken();
            utf8::encode(cp, output);
        }
    }
    flushToken();
    return output;
}

bool expectCommit(const char *input, const char *expected) {
    const std::string actual = commitOutput(input);
    if (actual == expected) return true;
    std::cerr << "commit " << input << " -> " << actual << ", expected " << expected << '\n';
    return false;
}

bool expectCommitWithSuffix(const char *input, char suffix, const char *expected) {
    const std::string actual = commitOutput(input) + suffix;
    if (actual == expected) return true;
    std::cerr << "commit " << input << suffix << " -> " << actual << ", expected " << expected << '\n';
    return false;
}

} // namespace

int main() {
    bool ok = true;

    ok = expect("nguowif", "người") && ok;
    ok = expect("nguowfi", "người") && ok;
    ok = expect("nguoiwf", "người") && ok;
    ok = expect("nguoifw", "người") && ok;
    ok = expect("thuowngr", "thưởng") && ok;
    ok = expect("thuongwr", "thưởng") && ok;
    ok = expect("thuongrw", "thưởng") && ok;

    ok = expect("uo", "uo") && ok;
    ok = expect("uow", "uơ") && ok;
    ok = expect("uocw", "ươc") && ok;
    ok = expect("uomw", "ươm") && ok;
    ok = expect("uonw", "ươn") && ok;
    ok = expect("uongw", "ương") && ok;
    ok = expect("uonhw", "ươnh") && ok;
    ok = expect("uopw", "ươp") && ok;
    ok = expect("uotw", "ươt") && ok;

    ok = expect("uocfw", "ườc") && ok;
    ok = expect("uomfw", "ườm") && ok;
    ok = expect("uonfw", "ườn") && ok;
    ok = expect("uongfw", "ường") && ok;
    ok = expect("uonhfw", "ườnh") && ok;
    ok = expect("uopfw", "ườp") && ok;
    ok = expect("uotfw", "ườt") && ok;
    ok = expect("uocwf", "ườc") && ok;
    ok = expect("uomwf", "ườm") && ok;
    ok = expect("uonwf", "ườn") && ok;
    ok = expect("uongwf", "ường") && ok;
    ok = expect("uonhwf", "ườnh") && ok;
    ok = expect("uopwf", "ườp") && ok;
    ok = expect("uotwf", "ườt") && ok;

    ok = expect("nguowifw", "người") && ok;
    ok = expect("thuongrww", "thưởng") && ok;

    ok = expect("soso", "số") && ok;
    ok = expect("sasa", "sấ") && ok;
    ok = expect("sasw", "sắ") && ok;
    ok = expect("sofo", "sồ") && ok;
    ok = expect("sojo", "sộ") && ok;
    ok = expect("soro", "sổ") && ok;
    ok = expect("soxo", "sỗ") && ok;
    ok = expect("sosw", "sớ") && ok;
    ok = expect("susw", "sứ") && ok;

    ok = expectVni("a1", "á") && ok;
    ok = expectVni("a2", "à") && ok;
    ok = expectVni("a3", "ả") && ok;
    ok = expectVni("a4", "ã") && ok;
    ok = expectVni("a5", "ạ") && ok;
    ok = expectVni("a6", "â") && ok;
    ok = expectVni("a8", "ă") && ok;
    ok = expectVni("e6", "ê") && ok;
    ok = expectVni("o6", "ô") && ok;
    ok = expectVni("o7", "ơ") && ok;
    ok = expectVni("u7", "ư") && ok;
    ok = expectVni("d9", "đ") && ok;
    ok = expectVni("a61", "ấ") && ok;
    ok = expectVni("a16", "ấ") && ok;
    ok = expectVni("tie61ng", "tiếng") && ok;
    ok = expectVni("tie62ng", "tiềng") && ok;
    ok = expectVniCommit("tie61ng", '/', "tiếng/") && ok;
    ok = expectVniCommit("duoc7", '/', "dươc/") && ok;
    ok = expectVniCommit("a1", '/', "á/") && ok;
    ok = expectVniCommit("a2", '/', "à/") && ok;
    ok = expectVniCommit("o2", '/', "ò/") && ok;
    ok = expectVniCommit("u7", '/', "ư/") && ok;
    ok = expectVniCommit("uoc7", '/', "ươc/") && ok;
    ok = expectVniCommit("uoc71", '/', "ước/") && ok;
    ok = expectVniCommit("duot7", '/', "dươt/") && ok;
    ok = expectVniCommit("duot75", '/', "dượt/") && ok;
    ok = expectVniCommit("hello7", '/', "hello7/") && ok;
    ok = expectVniCommit("1234567890", '/', "1234567890/") && ok;
    for (char tone = '1'; tone <= '5'; ++tone)
        ok = expectVniConsumed("duoc", tone) && ok;
    ok = expectVniConsumed("duoc", '6') && ok;
    ok = expectVniConsumed("duoc", '7') && ok;
    ok = expectVniConsumed("ba", '8') && ok;
    ok = expectVniConsumed("d", '9') && ok;
    for (char digit = '1'; digit <= '9'; ++digit)
        ok = expectVniLiteral("", digit) && ok;
    ok = expectVniLiteral("hello", '7') && ok;

    const std::pair<const char *, const char *> vniCases[] = {
        {"thuo73", "thuở"}, {"uoc7", "ươc"}, {"uoc71", "ước"},
        {"duot7", "dươt"}, {"duot75", "dượt"},
        {"tia1", "tía"}, {"tua3", "tủa"}, {"khoa1", "khóa"},
        {"hoa1", "hóa"}, {"xoa1", "xóa"}, {"khoe3", "khỏe"},
        {"luy5", "lụy"}, {"tuy4", "tũy"},
        {"hoat5", "hoạt"}, {"loat5", "loạt"}, {"khuay1", "khuáy"},
        {"khua1y", "khuáy"}, {"khu1ay", "khuáy"},
        {"khoai3", "khoải"}, {"toai1", "toái"}, {"loang1", "loáng"},
        {"thoang3", "thoảng"}, {"buan4", "buãn"}, {"buon2", "buòn"},
        {"buon1", "buón"}, {"khiet1", "khiét"},
        {"ho5", "họ"}, {"hoa5", "họa"}, {"hoat5", "hoạt"},
        {"khuy3", "khủy"}, {"khuyu3", "khuỷu"},
        {"kho1", "khó"}, {"khoa1", "khóa"}, {"khoai1", "khoái"},
        {"ho2", "hò"}, {"hoa2", "hòa"}, {"hoan2", "hoàn"}, {"hoang2", "hoàng"},
        {"ngu7o7i2", "người"}, {"nguo7i2", "người"}, {"nguoi72", "người"},
        {"nguoi27", "người"}, {"nguo2i7", "người"}, {"ngu2oi7", "người"},
        {"ngu2o7i", "người"},
        {"khua6y1", "khuấy"}, {"khuay61", "khuấy"}, {"khuay16", "khuấy"},
        {"khu1ay6", "khuấy"}, {"khua1y6", "khuấy"},
        {"d9u7o7ng2", "đường"}, {"du97o7ng2", "đường"}, {"du79o7ng2", "đường"},
        {"du7o97ng2", "đường"}, {"du7o7n9g2", "đường"}, {"du7o7ng92", "đường"},
        {"du7o7ng29", "đường"}, {"d9uo7ng2", "đường"}, {"du9o7ng2", "đường"},
        {"duo97ng2", "đường"}, {"duo79ng2", "đường"}, {"duo7n9g2", "đường"},
        {"duo7ng92", "đường"}, {"duo7ng29", "đường"}, {"d9uon7g2", "đường"},
        {"du9on7g2", "đường"}, {"duo9n7g2", "đường"}, {"duon97g2", "đường"},
        {"duong972", "đường"}, {"duong792", "đường"}, {"duong729", "đường"},
        {"d9uong72", "đường"}, {"du9ong72", "đường"}, {"duo9ng72", "đường"},
        {"duon9g72", "đường"},
    };
    for (const auto &[input, expected] : vniCases) ok = expectVni(input, expected) && ok;

    vicplex::CharBuffer modeBuffer;
    for (const char *p = "thanhf"; *p != '\0'; ++p) {
        vicplex::TelexTransformer::processKey(modeBuffer, *p);
    }
    modeBuffer.clear();
    for (const char *p = "tie61ng"; *p != '\0'; ++p) {
        vicplex::VniTransformer::processKey(modeBuffer, *p);
    }
    ok = expect("thanhf", "thành") && ok;
    std::string switched;
    vicplex::utf8::encodeAll(modeBuffer.data(), modeBuffer.size(), switched);
    ok = (switched == "tiếng") && ok;
    modeBuffer.clear();
    for (const char *p = "thanhf"; *p != '\0'; ++p) {
        vicplex::TelexTransformer::processKey(modeBuffer, *p);
    }
    switched.clear();
    vicplex::utf8::encodeAll(modeBuffer.data(), modeBuffer.size(), switched);
    ok = (switched == "thành") && ok;

    ok = expectCommitWithSuffix("thanhf", '/', "thành/") && ok;
    ok = expectCommitWithSuffix("ddungs", '@', "đúng@") && ok;
    ok = expectCommitWithSuffix("hai", '2', "hai2") && ok;
    ok = expectCommitWithSuffix("boons", '4', "bốn4") && ok;
    ok = expectCommitWithSuffix("chisn", '9', "chín9") && ok;
    ok = expectCommitWithSuffix("hoir", '?', "hỏi?") && ok;
    ok = expectCommitWithSuffix("sao", '*', "sao*") && ok;
    ok = expectCommitWithSuffix("vaf", '&', "và&") && ok;
    ok = expectCommitWithSuffix("thangw", '#', "thăng#") && ok;

    ok = expectCommitWithSuffix("faat", '3', "faat3") && ok;
    ok = expectCommitWithSuffix("rerou", '/', "rerou/") && ok;
    ok = expectCommitWithSuffix("faats", '3', "faats3") && ok;
    ok = expectCommitWithSuffix("rerou", '2', "rerou2") && ok;

    constexpr std::string_view corpus =
        "Buổi sáng, tôi uống một cốc nước ấm rồi bước ra ngoài, nhìn những đám mây trắng đang trôi chậm trên bầu trời. Người hàng xóm hỏi: “Hôm nay bạn khỏe không?” Tôi cười và đáp rằng mọi thứ khá ổn, dù công việc hơi bận và chiếc máy tính cũ đôi lúc chạy rất chậm. Ở góc bàn, một quyển sách, vài tờ giấy, 2 cây bút và chiếc USB vẫn nằm nguyên vị trí. Tôi mở file README.txt, kiểm tra lại dữ liệu, rồi viết nhanh vài dòng: “Tiếng Việt thật thú vị — vừa chính xác, vừa mềm mại, nhưng input method phải xử lý thật tốt.” Sau đó, tôi thử các từ như đường, thưởng, người, khuấy, khóa, khỏe, chuyện, chuyển, tưởng, hưởng, nghĩa, tiếng, cũng, vẫn, những và quyết định rằng bộ gõ này cần hoạt động reliably trong mọi tình huống.";
    const std::string telexCorpus = transformCorpus(corpus, false);
    const std::string vniCorpus = transformCorpus(corpus, true);
    ok = (telexCorpus == corpus) && ok;
    if (telexCorpus != corpus) std::cerr << "Telex corpus: " << telexCorpus << '\n';
    ok = (vniCorpus == corpus) && ok;
    if (vniCorpus != corpus) std::cerr << "VNI corpus: " << vniCorpus << '\n';
    ok = (telexCorpus == vniCorpus) && ok;

    return ok ? 0 : 1;
}
