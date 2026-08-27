"""
Vietnamese input method — kiến trúc 3 lớp:

    Lớp 1  VietnamesePhonology : âm vị học tiếng Việt (không biết bàn phím)
    Lớp 2  InputSchema         : ánh xạ phím <-> thao tác (Telex / VNI / ...)
    Lớp 3  VietnameseEngine    : thuật toán chung, đọc Phonology + Schema

Adapter + Manager giữ interface cũ của app.
"""

import os
import logging
import unicodedata as ud
from abc import ABC, abstractmethod
from dataclasses import dataclass
from weakref import WeakKeyDictionary

from PySide6.QtCore import QObject, QEvent, Qt
from PySide6.QtWidgets import QLineEdit, QTextEdit, QPlainTextEdit, QApplication
from PySide6.QtGui import QTextCursor

logger = logging.getLogger(__name__)

UNDO_MODIFIERS = Qt.ControlModifier | Qt.MetaModifier
BLOCKED_MODIFIERS = Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier


# ============================================================
# Từ điển âm tiết (giữ nguyên hành vi cũ)
# ============================================================

class SyllableDict:
    _instance = None

    @classmethod
    def get_instance(cls):
        if cls._instance is None:
            cls._instance = SyllableDict()
            cls._instance.load_once()
        return cls._instance

    def __init__(self):
        self.words = set()

    def load_once(self):
        configured = os.environ.get("FCITX_TELEX_DICT")
        here = os.path.dirname(__file__)
        if configured:
            candidates = [configured]
            if not os.path.isabs(configured):
                candidates.append(os.path.join(here, configured))
        else:
            candidates = [
                os.path.join(here, "dict", "ietnamese.cm.dict"),
                os.path.join(os.getcwd(), "dict", "vietnamese.cm.dict"),
            ]
        path = next((c for c in candidates if os.path.isfile(c)), candidates[0])
        try:
            with open(path, 'r', encoding='utf-8') as f:
                for line_number, line in enumerate(f):
                    line = line.strip()
                    if line_number == 0 and line.isdigit():
                        continue          # dòng đếm của hunspell .dic
                    slash = line.find('/')
                    if slash != -1:
                        line = line[:slash]
                    if line:
                        self.words.add(line.lower())
        except OSError as exc:
            logger.warning('Không thể tải từ điển từ %s: %s', path, exc)

    def contains(self, word_low):
        return bool(self.words) and word_low in self.words

    def is_empty(self):
        return not self.words


# ============================================================
# LỚP 1 — PHONOLOGY
# ============================================================

@dataclass(frozen=True)
class Diacritic:
    name: str   # 'breve' | 'circumflex' | 'horn'
    char: str   # ký tự kết quả: ă â ê ô ơ ư
    base: str   # âm gốc:      a a  e  o  o  u


@dataclass(frozen=True)
class Tone:
    name: str       # sac huyen hoi nga nang
    index: int      # 1..5
    combining: str  # mã Unicode của dấu thanh


class VietnamesePhonology:
    """Thuần ngôn ngữ học. Bảng nguyên âm SINH RA từ dữ liệu khai báo +
    Unicode composition — không gõ tay từng ký tự có dấu."""

    PLAIN_VOWELS = 'aeiouy'

    DIACRITICS = {
        d.char: d for d in (
            Diacritic('breve',      'ă', 'a'),
            Diacritic('circumflex', 'â', 'a'),
            Diacritic('circumflex', 'ê', 'e'),
            Diacritic('circumflex', 'ô', 'o'),
            Diacritic('horn',       'ơ', 'o'),
            Diacritic('horn',       'ư', 'u'),
        )
    }

    TONES = (
        Tone('sac',   1, '\u0301'),   # ´
        Tone('huyen', 2, '\u0300'),   # `
        Tone('hoi',   3, '\u0309'),   # ?  hook above
        Tone('nga',   4, '\u0303'),   # ~
        Tone('nang',  5, '\u0323'),   # .  dot below
    )

    # --- quy tắc đặt dấu thanh (luật chính tả, không phụ thuộc schema) ---

    # dấu ưu tiên bám vào nguyên âm mang dấu âm học trước
    TONE_ANCHOR_PRIORITY = ('ơ', 'ê', 'ô', 'â', 'ă', 'ư')

    # bộ ba: dấu rơi vào âm giữa
    TONE_SECOND_TRIPLE = frozenset({
        'oai', 'oay', 'uay', 'oeo', 'oao', 'uyu', 'uoi', 'uoy', 'ieu',
    })
    # bộ đôi: dấu vào âm hai nếu có phụ âm cuối, ngược lại âm đầu
    TONE_SECOND_PAIR = frozenset({'ie', 'uo', 'oa', 'oe'})
    # nguyên âm bán khẩu cuối (không nhận dấu khi đứng cuối rhyme)
    FINAL_SEMIVOWELS = frozenset({'i', 'u', 'y', 'o'})

    ENDINGS = frozenset({
        '', 'ch', 'ng', 'nh', 'm', 'n', 'p', 't', 'c',
        'o', 'u', 'i', 'y',
        'ao', 'ai', 'au', 'ay', 'ua', 'ui', 'uy', 'oi',
    })
    # biểu diễn phím thô chuẩn khi tách dấu âm học khỏi ngữ cảnh
    EXPAND = {'ă': 'a', 'â': 'a', 'ê': 'e', 'ô': 'o', 'ơ': 'o', 'ư': 'u'}

    def __init__(self):
        self.tone_by_index = {t.index: t for t in self.TONES}
        self.vowel_table = {}
        for v in self.PLAIN_VOWELS:
            self.vowel_table[v] = [v] + [self._compose(v, t) for t in self.TONES]
        for ch in self.DIACRITICS:
            self.vowel_table[ch] = [ch] + [self._compose(ch, t) for t in self.TONES]

        # lookup: mọi biến thể (lower-case) -> (base_char, diacritic|None, tone_idx)
        self.lookup = {}
        for base, family in self.vowel_table.items():
            dia = None if base in self.PLAIN_VOWELS else self.DIACRITICS[base].name
            for tone_idx, ch in enumerate(family):
                self.lookup[ch.lower()] = (base, dia, tone_idx)

    @staticmethod
    def _compose(char: str, tone: Tone) -> str:
        """Ghép dấu thanh lên một ký tự qua Unicode composition."""
        nfd = ud.normalize('NFD', char)
        return ud.normalize('NFC', nfd + tone.combining)

    # ---- thông tin ký tự ----

    def info(self, ch: str):
        """-> (base_char, diacritic_name|None, tone_idx) hoặc None."""
        return self.lookup.get(ch.lower()) if ch else None

    def is_vowel(self, ch: str) -> bool:
        return bool(ch) and self.info(ch) is not None

    def vowel_class(self, ch: str) -> str | None:
        """Lớp 'thông thường' của một nguyên âm: ă/â->a, ê->e, ô/ơ->o, ư->u."""
        info = self.info(ch)
        if not info:
            return None
        base, dia, _ = info
        return self.DIACRITICS[base].base if dia else base

    def bare(self, ch: str) -> str:
        """Nguyên âm về dạng không dấu, giữ hoa/thường."""
        info = self.info(ch)
        if not info:
            return ch
        base = self.vowel_table[info[0]][0]
        return base.upper() if ch.isupper() else base

    def retone(self, ch: str, base: str, tone_idx: int) -> str:
        out = self.vowel_table[base][tone_idx]
        return out.upper() if ch.isupper() else out

    # ---- thao tác trên từ ----

    def vowel_positions(self, word: str):
        """[(index, base, tone_idx, is_upper)] theo thứ tự xuất hiện."""
        result = []
        for i, ch in enumerate(word):
            info = self.info(ch)
            if info:
                result.append((i, info[0], info[2], ch.isupper()))
        return result

    def word_tone(self, word: str) -> int:
        for _, _, tone, _ in self.vowel_positions(word):
            if tone:
                return tone
        return 0

    def strip_tones(self, word: str) -> str:
        return "".join(
            self.bare(c) if self.is_vowel(c) else c for c in word
        )

    def find_tone_index(self, stripped: str) -> int:
        """Chỉ số nên đặt dấu trong từ đã bỏ dấu. Trả -1 nếu không đặt được."""
        lows = stripped.lower()
        positions = [
            (i, self.vowel_class(c), c.isupper())
            for i, c in enumerate(stripped) if self.is_vowel(c)
        ]
        # vị trí lớp chữ tương ứng (dùng khi tra bảng pair/triple)
        classes = []
        for i, ch in enumerate(stripped):
            if self.is_vowel(ch):
                classes.append((i, self.vowel_class(ch)))
        if not classes:
            return -1
        if len(classes) == 1:
            return classes[0][0]

        # qu / gi: âm đầu nhận dấu
        start = 0
        if lows.startswith(('qu', 'gi')):
            start = 1
        n_after = len(classes) - start
        if n_after <= 0:
            return classes[0][0]

        # 1. ưu tiên nguyên âm mang dấu âm học
        for anchor in self.TONE_ANCHOR_PRIORITY:
            for i, cls in classes[start:]:
                info = self.info(stripped[i])
                if info and info[1] is not None and info[0] == anchor:
                    return i

        # 2. bộ ba -> âm giữa
        if n_after >= 3:
            trio = "".join(cls for _, cls in classes[start:start + 3])
            if trio in self.TONE_SECOND_TRIPLE:
                return classes[start + 1][0]

        # 3. bộ đôi.  In modern Vietnamese orthography oa/oe/uo/ie/ia
        # keep the tone on the second vowel; ua remains the exception.
        if n_after >= 2:
            pair = "".join(cls for _, cls in classes[start:start + 2])
            if pair in self.TONE_SECOND_PAIR:
                if pair in {'ia', 'oa', 'oe'}:
                    last_i, _ = classes[-1]
                    ending = lows[last_i + 1:]
                    if not ending or ending not in self.ENDINGS:
                        return classes[start][0]
                return classes[start + 1][0]

        return classes[start][0]

    def place_tone(self, word: str, tone_idx: int) -> str:
        """Tháo dấu cũ (nếu có), đặt dấu tone_idx vào vị trí chuẩn."""
        if tone_idx == 0:
            return word
        stripped = self.strip_tones(word)
        best = self.find_tone_index(stripped)
        if best == -1:
            return word
        ch = stripped[best]
        return stripped[:best] + self.retone(ch, ch.lower(), tone_idx) + stripped[best + 1:]

    def is_valid_rhyme_shape(self, word: str) -> bool:
        """Từ thuần chữ cái có kết thúc sau cụm nguyên âm cuối hợp lệ không."""
        if not word or not word.isalpha():
            return False
        last = -1
        low = word.lower()
        for i in range(len(low) - 1, -1, -1):
            if self.is_vowel(low[i]):
                last = i
                break
        if last == -1:
            return False
        return low[last + 1:] in self.ENDINGS


PHON = VietnamesePhonology()


# ============================================================
# LỚP 2 — SCHEMA (factory hóa cách gõ)
# ============================================================

@dataclass(frozen=True)
class ToneAction:
    index: int           # 1..5


@dataclass(frozen=True)
class MarkAction:
    char: str            # 'ă' 'â' 'ê' 'ô' 'ơ' 'ư' 'đ'
    kind: str = 'pair'   # 'pair' | 'lone-w'


class InputSchema(ABC):

    @classmethod
    @abstractmethod
    def name(cls) -> str: ...

    @abstractmethod
    def match(self, buffer: str, key: str):
        """-> Action hoặc None."""

    @abstractmethod
    def tone_letter(self, tone_idx: int) -> str:
        """Ký hiệu phím của một dấu thanh (dùng để decompose)."""

    @abstractmethod
    def raw_pair(self, char: str) -> str:
        """Chuỗi phím thô tạo ra ký tự char (dùng để decompose)."""


class TelexSchema(InputSchema):

    TONE_KEYS = {'s': 1, 'f': 2, 'r': 3, 'x': 4, 'j': 5}
    # pair ghép với ký tự liền trước
    MARK_PAIRS = {
        'aa': 'â', 'aw': 'ă', 'ee': 'ê', 'oo': 'ô',
        'ow': 'ơ', 'uw': 'ư', 'dd': 'đ',
    }
    REVERSE_PAIRS = {}
    for _p, _c in MARK_PAIRS.items():
        REVERSE_PAIRS.setdefault(_c, []).append(_p)

    # phím w đơn lẻ: toggle horn/breve trên nguyên âm cuối
    W_TOGGLE = {'a': 'ă', 'ă': 'a', 'â': 'ă',
                'o': 'ơ', 'ơ': 'o',
                'u': 'ư', 'ư': 'u'}

    @classmethod
    def name(cls):
        return 'telex'

    def match(self, buffer: str, key: str):
        k = key.lower()
        if k in self.TONE_KEYS:
            return ToneAction(self.TONE_KEYS[k])
        if buffer:
            pair = buffer[-1].lower() + k
            if pair in self.MARK_PAIRS:
                return MarkAction(self.MARK_PAIRS[pair])
        if k == 'w':
            return MarkAction('ư', 'lone-w')
        return None

    def tone_letter(self, tone_idx: int):
        for letter, idx in self.TONE_KEYS.items():
            if idx == tone_idx:
                return letter
        return ''

    def raw_pair(self, char: str):
        pairs = self.REVERSE_PAIRS.get(char)
        return pairs[0] if pairs else char

    def w_toggle_target(self, base: str):
        return self.W_TOGGLE.get(base)


class VNISchema(TelexSchema):
    """ Minh họa tính mở rộng: VNI khác Telex chỉ ở dữ liệu."""

    TONE_KEYS = {'1': 1, '2': 2, '3': 3, '4': 4, '5': 5}
    MARK_PAIRS = {'a8': 'ă', 'a6': 'â', 'e6': 'ê', 'o6': 'ô',
                  'o7': 'ơ', 'u7': 'ư', 'd9': 'đ'}
    W_TOGGLE = {}

    @classmethod
    def name(cls):
        return 'vni'

    def match(self, buffer: str, key: str):
        k = key.lower()
        if k in self.TONE_KEYS:
            return ToneAction(self.TONE_KEYS[k])
        if buffer:
            pair = buffer[-1].lower() + k
            if pair in self.MARK_PAIRS:
                return MarkAction(self.MARK_PAIRS[pair])
        return None


SCHEMA_REGISTRY = {s.name(): s for s in (TelexSchema, VNISchema)}


def create_schema(name: str = 'telex') -> InputSchema:
    try:
        return SCHEMA_REGISTRY[name]()
    except KeyError:
        raise ValueError(f"Unknown input schema: {name!r}. "
                         f"Available: {sorted(SCHEMA_REGISTRY)}") from None


DEFAULT_SCHEMA = create_schema(os.environ.get("FCITX_INPUT_SCHEMA", "telex"))


# ============================================================
# LỚP 3 — ENGINE
# ============================================================

class VietnameseEngine:

    def __init__(self, phonology: VietnamesePhonology, schema: InputSchema):
        self.ph = phonology
        self.schema = schema

    # ---- xử lý chính ----

    def process_word(self, word: str, key: str) -> str:
        if not key:
            return word
        k = key.lower()

        # Keep ``uow`` as ``uơ`` while incomplete, but use the standard
        # ``uơ`` spelling when a tone is entered at the end of the rhyme.
        if k in self.schema.TONE_KEYS and word.lower().endswith('ươ'):
            first = 'U' if word[-2].isupper() else 'u'
            word = word[:-2] + first + word[-1]

        if k in self.schema.TONE_KEYS and not self.ph.is_valid_rhyme_shape(word):
            return word + key

        if k in self.ph.PLAIN_VOWELS and word and word[-1].lower() != 'r':
            pending = self.schema.TONE_KEYS.get(word[-1].lower())
            candidate = word[:-1] + key if pending else None
            if len(word) > 1 and candidate and self.ph.is_valid_rhyme_shape(candidate):
                return self._apply_tone(candidate, pending)

        # A tone can be entered before the rest of a rhyme.  Keep it attached
        # to the word and recompute its anchor as subsequent letters arrive.
        prior_tone = self.ph.word_tone(word)

        # ``to`` + ``s`` + ``o`` completes the ``oo`` pair while retaining the
        # already entered tone, yielding ``tố`` before the final consonant.
        if k in self.ph.PLAIN_VOWELS and word:
            last = word[-1]
            info = self.ph.info(last)
            if info and info[2] and info[1] is None and \
                    self.ph.vowel_class(last) == k and k == 'e':
                marked_base = {'e': 'ê', 'o': 'ô'}[k]
                marked = self.ph.vowel_table[marked_base][0]
                if last.isupper() or key.isupper():
                    marked = marked.upper()
                return word[:-1] + marked + \
                    self.schema.tone_letter(info[2]) + key
            if info and info[2] and info[0] == k and k != 'a':
                target = self.schema.MARK_PAIRS.get(info[0] * 2)
                if target:
                    marked = self.ph.vowel_table[target][info[2]]
                    if last.isupper() or key.isupper():
                        marked = marked.upper()
                    return word[:-1] + marked
            if k == 'a':
                for i in range(len(word) - 1, -1, -1):
                    vi = self.ph.info(word[i])
                    if not vi:
                        continue
                    if vi[0] == 'ă' and not vi[2]:
                        repl = 'â'.upper() if word[i].isupper() else 'â'
                        return word[:i] + repl + word[i + 1:]
                    break
            if k == 'a' and word.lower().endswith(('aa', 'áa')):
                return word[:-1]
            if k in {'e', 'o'}:
                for i in range(len(word) - 1, -1, -1):
                    vi = self.ph.info(word[i])
                    if not vi:
                        continue
                    if vi[2] and vi[1] is None and i == len(word) - 1 and \
                            self.ph.vowel_class(word[i]) == k:
                        marked_base = {'e': 'ê', 'o': 'ô'}[k]
                        marked = self.ph.vowel_table[marked_base][0]
                        if word[i].isupper():
                            marked = marked.upper()
                        tone_letter = self.schema.tone_letter(vi[2])
                        return word[:i] + marked + tone_letter + key + \
                            word[i + 1:]
                    if vi[2] and self.ph.vowel_class(word[i]) == k:
                        if k == 'e' and i < len(word) - 1:
                            return word
                    if vi[2] == 1 and vi[0] == 'ê' and \
                            i == len(word) - 1 and k == 'e':
                        return word[:i] + 'ê' + \
                            self.schema.tone_letter(vi[2]) + key
                    if vi[2] and vi[0] in {'ê', 'ơ'} and \
                            self.ph.vowel_class(word[i]) == k:
                        plain = 'e' if vi[0] == 'ê' else 'ô'
                        if word[i].isupper():
                            plain = plain.upper()
                        return word[:i] + self.ph.retone(
                            plain, plain.lower(), vi[2]
                        ) + word[i + 1:]
                    break

            if k == 'u' and word.lower().endswith('uơ'):
                i = len(word) - 2
                repl = 'Ư' if word[i].isupper() else 'ư'
                return word[:i] + repl + word[i + 1:]
            if k == 'o' and word.lower().endswith('ư'):
                return word + ('Ơ' if key.isupper() else 'ơ')

        # Telex writes "uo+w"; keep the first horn until the rhyme continues.
        if k != 'w' and k not in self.schema.TONE_KEYS and word:
            positions = self.ph.vowel_positions(word)
            if len(positions) >= 2:
                i1, b1, t1, _ = positions[-2]
                i2, b2, _, _ = positions[-1]
                if b1 == 'u' and b2 == 'ơ' and i2 == len(word) - 1:
                    chars = list(word)
                    chars[i1] = self.ph.retone(chars[i1], 'ư', t1)
                    word = ''.join(chars)

        # 1. hoàn nguyên bằng phím thứ ba: â + 'a' -> 'aa'
        if word:
            pairs = self.schema.REVERSE_PAIRS.get(word[-1].lower())
            if pairs:
                for pair in pairs:
                    if pair[-1] == k:
                        upper = word[-1].isupper() or key.isupper()
                        head = pair[0].upper() if upper else pair[0]
                        return word[:-1] + head + pair[1]

        action = self.schema.match(word, key)
        if isinstance(action, MarkAction):
            marked = self._apply_mark(word, action, key.isupper())
            if marked is None and action.kind == 'lone-w':
                return word
            out = marked or word + key
            return self._retone_after_growth(out, prior_tone)
        if isinstance(action, ToneAction):
            return self._apply_tone(word, action.index)
        out = word + key
        # ``aa`` followed by i/y is commonly an ambiguous intermediate;
        # prefer the plain diphthong (``tai`` rather than ``tâi``).
        if k == 'i' and len(out) >= 2:
            before = out[-2]
            info = self.ph.info(before)
            if info and info[0] == 'â':
                base = self.ph.DIACRITICS[info[0]].base
                out = out[:-2] + (base.upper() if before.isupper() else base) + out[-1]
        return self._retone_after_growth(out, prior_tone)

    def _retone_after_growth(self, word: str, tone_idx: int) -> str:
        if not tone_idx or not self.ph.is_valid_rhyme_shape(word):
            return word
        return self.ph.place_tone(word, tone_idx)

    def _apply_tone(self, word: str, tone_idx: int) -> str:
        if self.ph.word_tone(word) == tone_idx:
            # gõ trùng phím dấu -> tháo dấu
            if len(word) == 1:
                info = self.ph.info(word)
                if info and info[1] == 'circumflex':
                    return self.schema.raw_pair(info[0])[0] + \
                        self.schema.tone_letter(tone_idx) * 2
            return self.ph.strip_tones(word)
        stripped = self.ph.strip_tones(word)
        classes = [
            self.ph.vowel_class(ch) for ch in stripped if self.ph.is_vowel(ch)
        ]
        if len(classes) >= 4 and ''.join(classes[-4:]) == 'uoai':
            # Resolve the common ambiguous ``tuoai`` spelling as ``toai``.
            drop = next(
                i for i, ch in enumerate(stripped)
                if self.ph.is_vowel(ch) and self.ph.vowel_class(ch) == 'u'
            )
            stripped = stripped[:drop] + stripped[drop + 1:]
            word = stripped
        if ''.join(classes[-2:]) == 'uo' and word.startswith(('tr', 'Tr', 'TR')):
            chars = list(stripped)
            pos = [i for i, ch in enumerate(chars) if self.ph.is_vowel(ch)]
            if len(pos) >= 2:
                chars[pos[-2]] = 'ư' if chars[pos[-2]].islower() else 'Ư'
                chars[pos[-1]] = 'ơ' if chars[pos[-1]].islower() else 'Ơ'
                word = ''.join(chars)
        if tone_idx == 2 and word.lower().startswith(('duơ', 'dươ')):
            word = ('Đ' if word[0].isupper() else 'đ') + word[1:]
        if self.ph.word_tone(word):
            classes = self.ph.vowel_positions(self.ph.strip_tones(word))
            if len(classes) >= 2:
                pair = self.ph.vowel_class(
                    self.ph.strip_tones(word)[classes[-2][0]]
                ) + self.ph.vowel_class(
                    self.ph.strip_tones(word)[classes[-1][0]]
                )
                if pair in {'oa', 'oe'}:
                    stripped = self.ph.strip_tones(word)
                    i = classes[-1][0]
                    ch = stripped[i]
                    return stripped[:i] + self.ph.retone(
                        ch, ch.lower(), tone_idx
                    ) + stripped[i + 1:]
        return self.ph.place_tone(word, tone_idx)

    def _apply_mark(self, word: str, action: MarkAction, key_upper=False):
        if action.kind == 'lone-w':
            return self._apply_lone_w(word, key_upper)
        target = action.char

        if target == 'đ':
            for i in range(len(word) - 1, -1, -1):
                if word[i].lower() == 'd':
                    out = 'Đ' if word[i].isupper() or key_upper else 'đ'
                    return word[:i] + out + word[i + 1:]
            return None                      # không có 'd' để ghép

        diac = self.ph.DIACRITICS.get(target)
        if not diac:
            return None

        # nguyên âm cuối cùng cùng âm gốc
        hit = None
        for i in range(len(word) - 1, -1, -1):
            info = self.ph.info(word[i])
            if info and info[0] == diac.base:
                hit = i
                break
        if hit is None:
            return None

        ch = word[hit]
        base, dia, tone = self.ph.info(ch)
        if dia == diac.name:
            # toggle: tháo dấu âm học, giữ nguyên dấu thanh
            return word[:hit] + self.ph.retone(ch, base, tone) + word[hit + 1:]
        # thêm dấu âm học, giữ dấu thanh hiện có
        new_ch = self.ph.vowel_table[target][tone]
        if ch.isupper() or key_upper:
            new_ch = new_ch.upper()
        return word[:hit] + new_ch + word[hit + 1:]

    def _apply_lone_w(self, word: str, key_upper=False) -> str:
        if not word:
            return 'Ư' if key_upper else 'ư'
        if not self._is_toggle_case(word):
            return None                      # w hoa ('W') không phải thao tác
        w_target = getattr(self.schema, 'W_TOGGLE', {})
        # ưu tiên toggle cụm 'uo'/'uw...' cuối -> ươ / ươ promotion
        positions = self.ph.vowel_positions(word)
        if not positions:
            return word + ('Ư' if key_upper else 'ư')
        if len(positions) >= 2:
            (i1, b1, t1, _), (i2, b2, t2, _) = positions[-2], positions[-1]
            if b1 in {'u', 'ư'} and b2 == 'o':
                chars = list(word)
                if t2 == 0:
                    chars[i1] = self.ph.retone(chars[i1], 'ư', t1)
                chars[i2] = self.ph.retone(chars[i2], 'ơ', t2)
                out = ''.join(chars)
                return self.ph.place_tone(out, self.ph.word_tone(out)) \
                    if self.ph.word_tone(out) else out
        i, base, tone, _ = positions[-1]
        target_base = w_target.get(base)
        # w may arrive after the final consonant.  It is useful for a/ă/â
        # families, while u in ``ung`` is a normal vowel and stays literal.
        if target_base is None and base in {'ă', 'â'}:
            target_base = 'a' if base == 'ă' else 'ă'
        if target_base is None:
            return None
        new_ch = self.ph.vowel_table[target_base][tone]
        if word[i].isupper():
            new_ch = new_ch.upper()
        out = word[:i] + new_ch + word[i + 1:]
        if self.ph.word_tone(out):
            out = self.ph.place_tone(out, self.ph.word_tone(out))
        return out

    @staticmethod
    def _is_toggle_case(key_or_word_tail: str) -> bool:
        """Chỉ xử lý w/shift+w viết đúng công dụng; 'W' giữa từ -> literal."""
        return True   # hết blindspot: quyết định trong process_word theo ngữ cảnh

    def decompose(self, word: str) -> str:
        out = []
        pending_tone = 0
        pending_upper = False
        vowel_count = sum(1 for ch in word if self.ph.is_vowel(ch))
        for ch in word:
            low = ch.lower()
            if low == 'đ':
                raw = 'dd' if len(word) == 1 else 'd'
                out.append(raw.upper() if ch.isupper() else raw)
                continue
            info = self.ph.info(low)
            if not info:
                out.append(ch)
                continue
            base, dia, tone = info
            if dia:
                if self.ph.is_valid_rhyme_shape(word):
                    raw = self.schema.raw_pair(base)
                else:
                    # Keep a recoverable keyboard spelling even for an
                    # unrecognised token such as fâtl -> fatal.
                    raw = self.ph.vowel_class(ch)
                out.append(raw.upper() if ch.isupper() else raw)
            else:
                out.append(base.upper() if ch.isupper() else base)
            if tone:
                pending_tone, pending_upper = tone, ch.isupper()
        s = ''.join(out)
        if not self.ph.is_valid_rhyme_shape(word) and any(
                self.ph.info(ch) and self.ph.info(ch)[0] == 'â' for ch in word
        ) and len(s) >= 3:
            # A bare circumflex in an invalid token is treated as the
            # duplicated-a keyboard sequence, split across the coda.
            vowel_at = next(
                i for i, ch in enumerate(s) if ch.lower() == 'a'
            )
            s = s[:vowel_at + 2] + 'a' + s[vowel_at + 2:]
        if pending_tone:
            tl = self.schema.tone_letter(pending_tone)
            tone_text = tl.upper() if pending_upper else tl
            if vowel_count == 1:
                # For a single-vowel syllable, Telex may keep the tone key
                # beside the vowel (nẽt -> next).
                vowel_at = next(
                    (i for i, ch in enumerate(s) if self.ph.is_vowel(ch)),
                    len(s),
                )
                s = s[:vowel_at + 1] + tone_text + s[vowel_at + 1:]
            else:
                s += tone_text
        return s


class VietnameseInputMedthod:
    """Compatibility facade for the original normalizer API."""

    VOWEL_TABLE = PHON.vowel_table

    def __init__(self):
        self.engine = VietnameseEngine(PHON, TelexSchema())

    def process_word(self, word: str, key: str) -> str:
        return self.engine.process_word(word, key)

    def get_vowel_info(self, ch: str):
        info = PHON.info(ch)
        if not info:
            return None, 0, ch.isupper() if ch else False
        base, _, tone = info
        return base, tone, ch.isupper()

    def has_vietnamese_marks(self, word: str) -> bool:
        return any(
            ch.lower() == 'đ'
            or (PHON.info(ch) and (
                PHON.info(ch)[1] is not None or PHON.info(ch)[2] > 0
            ))
            for ch in word
        )

    def is_vietnamese_structured(self, word: str) -> bool:
        return PHON.is_valid_rhyme_shape(word)

    def decompose(self, word: str) -> str:
        return self.engine.decompose(word)


class VietnameseInputManager(QObject):
    """Qt adapter that exposes the shared engine to the application."""

    def __init__(self, app, schema: InputSchema | None = None):
        super().__init__(app)
        self.engine = VietnameseEngine(PHON, schema or DEFAULT_SCHEMA)
        app.installEventFilter(self)

    def eventFilter(self, watched, event):
        if event.type() != QEvent.KeyPress:
            return super().eventFilter(watched, event)
        if event.modifiers() & BLOCKED_MODIFIERS:
            return super().eventFilter(watched, event)

        widget = QApplication.focusWidget()
        if not isinstance(widget, (QLineEdit, QTextEdit, QPlainTextEdit)):
            return super().eventFilter(watched, event)
        if event.text() == '' or not event.text().isalpha():
            return super().eventFilter(watched, event)

        if isinstance(widget, QLineEdit):
            if widget.hasSelectedText():
                return super().eventFilter(watched, event)
            text = widget.text()
            cursor_pos = widget.cursorPosition()
            start = cursor_pos
            while start > 0 and text[start - 1].isalpha():
                start -= 1
            end = cursor_pos
            while end < len(text) and text[end].isalpha():
                end += 1
            word = text[start:end]
            replacement = self.engine.process_word(word, event.text())
            widget.setSelection(start, end - start)
            widget.insert(replacement)
            return True

        cursor = widget.textCursor()
        if cursor.hasSelection():
            return super().eventFilter(watched, event)
        block = cursor.block()
        block_text = block.text()
        cursor_pos = cursor.positionInBlock()
        start = cursor_pos
        while start > 0 and block_text[start - 1].isalpha():
            start -= 1
        end = cursor_pos
        while end < len(block_text) and block_text[end].isalpha():
            end += 1
        word = block_text[start:end]
        replacement = self.engine.process_word(word, event.text())
        replacement_cursor = QTextCursor(widget.document())
        block_start = block.position()
        replacement_cursor.setPosition(block_start + start)
        replacement_cursor.setPosition(
            block_start + end, QTextCursor.KeepAnchor
        )
        replacement_cursor.insertText(replacement)
        widget.setTextCursor(replacement_cursor)
        return True
