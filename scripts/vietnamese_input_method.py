
from vietnamese_phonology import VietnamesePhonology
from input_schema import TelexSchema, VNISchema, Action


class VietnameseEngine:
    """
    State Orchestrator.
    Trách nhiệm:
    1. Tiếp nhận phím thô (_raw).
    2. Hỏi InputSchema để dịch phím thành Action (ToneAction / MarkAction)[cite: 1].
    3. Quản lý trạng thái Toggle (áp dụng / hoàn tác dấu) qua API của VietnamesePhonology[cite: 1, 2].
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

        word = self._phon.reconstruction(word)
        word += literal

        self._base = word
        #print(f'Base: {self._base}')

    def get_word(self) -> str:
        return self._base

    def commit(self) -> str:
        result = self._base
        self._reset()
        return result

    def _reset(self):
        self._base = ""