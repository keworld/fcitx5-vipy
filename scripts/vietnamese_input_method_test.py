import unittest

from vietnamese_input_method import VietnameseInputMedthod


class VietnameseTelexNormalizerTest(unittest.TestCase):
    def setUp(self):
        self.normalizer = VietnameseInputMedthod()

    def feed(self, keys: str) -> str:
        word = ""
        for key in keys:
            word = self.normalizer.process_word(word, key)
        return word

    # ----------------------------------------------------------------------
    # 1. CORE COMPOSITION & CASE HANDLING
    # ----------------------------------------------------------------------
    def test_empty_key_is_noop(self):
        self.assertEqual(self.normalizer.process_word("abc", ""), "abc")

    def test_all_vowel_families_and_tones_are_recognized(self):
        for base, family in self.normalizer.VOWEL_TABLE.items():
            for tone, character in enumerate(family):
                with self.subTest(base=base, tone=tone):
                    detected_base, detected_tone, is_upper = (
                        self.normalizer.get_vowel_info(character)
                    )
                    self.assertEqual(detected_base, base)
                    self.assertEqual(detected_tone, tone)
                    self.assertFalse(is_upper)

                    upper_base, upper_tone, is_upper = (
                        self.normalizer.get_vowel_info(character.upper())
                    )
                    self.assertEqual(upper_base, base)
                    self.assertEqual(upper_tone, tone)
                    self.assertTrue(is_upper)

    def test_has_vietnamese_marks(self):
        marked = ("á", "â", "ă", "ê", "ô", "ơ", "ư", "đ")
        for word in marked:
            with self.subTest(word=word):
                self.assertTrue(self.normalizer.has_vietnamese_marks(word))
        for word in ("a", "hello", "dd", "uw"):
            with self.subTest(word=word):
                self.assertFalse(self.normalizer.has_vietnamese_marks(word))

    def test_decompose(self):
        self.assertEqual(self.normalizer.decompose("tiếng"), "tieengs")
        self.assertEqual(self.normalizer.decompose("đường"), "duwowngf")
        self.assertEqual(self.normalizer.decompose("ƯỜ"), "UWOWF")
        self.assertEqual(self.normalizer.decompose("Đ"), "DD")
        self.assertEqual(self.normalizer.decompose("lĩnu"), "linux")
        self.assertEqual(self.normalizer.decompose("nẽt"), "next")
        self.assertEqual(self.normalizer.decompose("fâtl"), "fatal")

    def test_structured_syllable_validation(self):
        valid_words = ("ba", "ban", "bánh", "hoa", "hoang", "duong", "nghiêng")
        for word in valid_words:
            with self.subTest(word=word):
                self.assertTrue(self.normalizer.is_vietnamese_structured(word))

        invalid_words = ("", "bbb", "banx", "a1", "htkp")
        for word in invalid_words:
            with self.subTest(word=word):
                self.assertFalse(self.normalizer.is_vietnamese_structured(word))

    def test_basic_vowel_and_consonant_composition(self):
        cases = {
            "aa": "â",
            "aw": "ă",
            "ee": "ê",
            "oo": "ô",
            "ow": "ơ",
            "uw": "ư",
            "dd": "đ",
            "w": "ư",  # Standalone 'w' shortcut for 'ư'
            "AA": "Â",
            "Aa": "Â",
            "aA": "Â",
            "AW": "Ă",
            "Aw": "Ă",
            "EE": "Ê",
            "OO": "Ô",
            "DD": "Đ",
            "Dd": "Đ",
            "dD": "Đ",
            "Dduowngf": "Đường",
            "as": "á",
            "af": "à",
            "ar": "ả",
            "ax": "ã",
            "aj": "ạ",
            "tieengs": "tiếng",
            "Vieejt": "Việt",
            "dduowngf": "đường",
            "hoas": "hóa",      # hoặc "hoá" tùy chuẩn engine
            "hoans": "hoán",
            "quas": "quá",      # Dấu rơi vào 'a', không phải 'qúa'
            "quans": "quán",
            "gias": "giá",      # Dấu rơi vào 'a', không phải 'gía'
            "giaf": "già",
            "chuyeern": "chuyển",
            "quynhf": "quỳnh",
            "thuyfs": "thúy",
            "khuys": "khúy",
            "oais": "oái",
            "hfoang": "hoàng",
            "hoafng": "hoàng",
            "hoangf": "hoàng",
            "toosn": "tốn",
            "toson": "tốn",
            "tfay": "tày",
            "tsoan": "toán",
            "tjinh": "tịnh",
            "txinh": "tĩnh",
            "trinhr": "trỉnh",
            "trinh": "trinh",
            "tsa": "tá",
            "ass": "a",
            "tieengss": "tiêng",
            "asf": "à",
            "tieengsf": "tiềng",
            "oof": "ồ",
            "ooff": "off",     # Gõ 'f' lần 2 để trả lại tiếng Anh
            "chees": "chế",
            "cheese": "chêse",  # Tùy cơ chế restore của engine
            "ddad": "đad",
            "abcs": "abcs",
            "xyzf": "xyzf",
            "strr": "strr",
            "aw": "ă",
            "ow": "ơ",
            "uw": "ư",
            "uow": "uơ",
            "uown": "ươn",
            "duowng": "dương",
            "dduowngf": "đường",
            "aaf": "ầ",
            "oof": "ồ",
            "uofn": "uòn",
            "nguowfi": "người",
            "vowfi": "vời",
            "chuyeenfs": "chuyến",  # Đổi dấu trực tiếp trên cụm nguyên âm
            "hoasf": "hoà",
            "oasf": "oà",
            "uowis": "ưới",
            "uowif": "ười",
            "mowfi": "mời",
            "gowfi": "gời",
            "treen": "trên",
            "treens": "trến",
            "treenf": "trền",
            "troon": "trôn",
            "traan": "trân",
            "truongf": "trường",
            "proo": "prô",
            "croom": "crôm",
            # Chuỗi gõ với phím 's' (dấu sắc)
            "toosi": "tối",
            "taais": "tái",
            "tieesng": "tiếng",
            "tuoais": "toái",

            # Chuỗi gõ với phím 'f' (dấu huyền)
            "tooif": "tồi",
            "toaif": "toài",
            "tieefng": "tiềng",
            "ddaafy": "đầy",

            # Chuỗi gõ với phím 'r' (dấu hỏi)
            "tooir": "tổi",
            "tuoair": "toải",

            # Chuỗi gõ với phím 'x' (dấu ngã)
            "tooix": "tỗi",
            "toaix": "toãi",

            # Chuỗi gõ với phím 'j' (dấu nặng)
            "tooij": "tội",
            "toaij": "toại",
            "tieejng": "tiệng",
            # Vần 'ai'
            "nhaij": "nhại",
            "hais": "hái",
            "haif": "hài",
            "hair": "hải",
            "haix": "hãi",

            # Vần 'ao'
            "baos": "báo",
            "baoj": "bạo",
            "caif": "cài",

            # Vần 'oi'
            "nois": "nói",
            "noij": "nọi",

            # Vần 'au' / 'ay'
            "shaus": "sháu",
            "chayj": "chạy",

            # Vần 'oe' / 'ia' / 'ua'
            "choes": "chóe",
            "chias": "chía",
            "muaj": "mụa",
            "suoots": "suốt",
            "uoost": "uốt",
            "cuoons": "cuốn",
            "tuoost": "tuốt",
            "luoots": "luốt",
            "suowst": "sướt",
            "duowngf": "đường",
            "muowsn": "mướn",
            "tuowungf": "tường",
            "huowngs": "hướng",
            "buowsc": "bước",
            "uwocs": "ước",  # Uw + o + c + s -> ước
            "uowsc": "ước",  # U + o + w + s + c -> ước
            "huowst": "hướt",  # Phân biệt hướt vs huốt
            "huoots": "huốt",
            "daayf": "dầy",
            "dungwj": "dựng",
            "ddieefu": "điều",
            "ddaatws": "đắt",
            "dawtsw": "dát",
            "thieeus": "thiếu",
            "dawnga": "dâng",
            "howro": "hổ",
            "heefe": "hè",
            "hasaa": "há",
            "hefnee": "hèn",
            "husww": "hú",
            "tenfee": "tèn",
            "thuowr": "thuở",
            "thuorw": "thuở",
        }
        for keys, expected in cases.items():
            with self.subTest(keys=keys):
                self.assertEqual(self.feed(keys), expected)

if __name__ == "__main__":
    unittest.main()