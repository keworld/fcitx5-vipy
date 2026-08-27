#ifndef VICPLEX_CHAR_UTIL_HPP
#define VICPLEX_CHAR_UTIL_HPP

namespace vipy {

    class CharUtil {
    public:
        static bool isAsciiLetter(char32_t cp) {
            return (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z');
        }

        static char asciiLower(char32_t cp) {
            return static_cast<char>((cp >= 'A' && cp <= 'Z') ? cp + 0x20 : cp);
        }

        static bool isVowel(char32_t cp) {
            switch (cp) {
            case 'a': case 'e': case 'i': case 'o': case 'u': case 'y':
            case 'A': case 'E': case 'I': case 'O': case 'U': case 'Y':
            case 0x00E1: case 0x00E0: case 0x1EA3: case 0x00E3: case 0x1EA1:
            case 0x00E2: case 0x0103: case 0x00E9: case 0x00E8: case 0x00EA:
            case 0x00ED: case 0x00EC: case 0x00F3: case 0x00F2: case 0x00F4:
            case 0x01A1: case 0x00FA: case 0x00F9: case 0x01B0:
            case 0x0111:
                return true;
            default:
                return false;
            }
        }
    };

} // namespace vipy

#endif // VICPLEX_CHAR_UTIL_HPP