
from vietnamese_phonology import VietnamesePhonology
from input_schema import (
    TelexSchema, VNISchema, Action
)


class VietnameseEngine:
    """
    State Orchestrator.
    Trách nhiệm:
    1. Tiếp nhận phím thô (_raw).
    2. Hỏi InputSchema để dịch phím thành Action (ToneAction / MarkAction).
    3. Quản lý trạng thái Toggle (áp dụng / hoàn tác dấu) qua API của VietnamesePhonology.
    """

    def __init__(self, schema_name: str = "telex"):
        self._phon = VietnamesePhonology()
        self.assign_schema(schema_name)

    def assign_schema(self, name: str = "telex"):
        self._schema = VNISchema() if name.lower() == "vni" else TelexSchema()
        self._reset()

    def feed(self, key: str):
        word = self._base
        literal = ""

        if key == "\b":
            if word:
                word = word[:-1]
        elif self._lone_w_pending and key.lower() == 'w':
            # ww -> escape: xóa ư vừa chèn, xuất w thường (chuẩn Telex)
            word = word[:-1]
            literal = 'W' if key.isupper() else 'w'
        else:
            action = self._schema.match(word, key)
            match action:
                case Action(type='none', value=none_val):
                    word += key

                case Action(type='mark', value=mark_val):
                    word = self._phon.place_mark(word, mark_val)

                case Action(type='tone', value=tone_val):
                    word = self._phon.place_tone(word, tone_val)

                case Action(type='toggle_tone', value=tone_val):
                    word = self._phon.strip_tone(word)
                    literal = key

                case Action(type='toggle_mark', value=mark_val):
                    word = self._phon.strip_mark(word, mark_val)
                    literal = key

                case Action(type='lone_w', value=w_upper):
                    ch = 'Ư' if w_upper == 1 else 'ư'
                    word += ch
                    self._lone_w_pending = True
                    word = self._phon.reconstruction(word)
                    word += literal
                    self._base = word
                    return  # giữ cờ, không rơi xuống phần reset dưới

        word = self._phon.reconstruction(word)
        word += literal

        self._base = word
        self._lone_w_pending = False
        #print(f'Base: {self._base}')

    def get_word(self) -> str:
        return self._base

    def commit(self) -> str:
        result = self._base
        self._reset()
        return result

    def _reset(self):
        self._base = ""
        self._lone_w_pending = False
