# Module: `input_schema.py` — Tài liệu Đặc tả cho AI

## 1. Mục đích Module

Module định nghĩa **lược đồ nhập liệu (input schema)** cho bộ gõ tiếng Việt, chuyển đổi giữa **chuỗi phím người dùng gõ** (key sequence, vd Telex `aaesf`) và **hành động âm vị học** (đặt dấu phụ `ă â ê ô ơ ư đ`, đặt dấu thanh sắc/huyền/hỏi/ngã/nặng).

Module **không tự biến đổi chữ** — nó chỉ *suy luận* `Action`; việc áp dụng action lên buffer chữ do Engine thực hiện qua module `VietnamesePhonology` (tham chiếu `input_method.vietnamese_phonology`, lớp `VietnamesePhonology`).

**Hai schema cài đặt sẵn:**
- `TelexSchema` (`name() == 'telex'`) — hỗ trợ thêm phím `w` độc lập (toggle horn).
- `VNISchema` (`name() == 'vni'`) — kế thừa từ `TelexSchema`, chỉ override bảng dữ liệu.

## 2. Mô hình dữ liệu (Action)

Engine phải xử lý hai loại action:

| Dataclass | Trường | Ý nghĩa |
|---|---|---|
| `ToneAction` | `index` (1–5) | Đặt dấu thanh: 1=sắc, 2=huyền, 3=hỏi, 4=ngã, 5=nặng. Engine gọi `Phonology.place_tone(buffer, index)`. |
| `MarkAction` | `index` (1–4), `mod: str` | Đặt dấu phụ: 1=breve(`ă`), 2=circumflex(`â ê ô`), 3=horn(`ơ ư`), 4=stroke(`đ`). `mod` là **phím chuẩn hóa** (`'a'/'e'/'o'/'w'/'d'`) thống nhất giữa mọi schema — Engine dùng `mod` để biết nguyên tắc gỡ/đảo khi user gõ lại. Engine gọi `Phonology.place_mark(buffer, index)` (hoặc `place_marks` cho action ghép `ươ`). |

`Action = Union[ToneAction, MarkAction]`.

**Bảng ánh xạ ký tự → mark (hằng số dùng chung):**
`CHAR_TO_MARK = {'ă':1, 'â':2, 'ê':2, 'ô':2, 'ơ':3, 'ư':3, 'đ':4}`

## 3. Bảng dữ liệu schema

### TelexSchema
```python
TONE_KEYS  = {'s':1, 'f':2, 'r':3, 'x':4, 'j':5}
MARK_PAIRS = {          # pair -> (ký tự kết quả, modifier chuẩn)
    'aa': ('â','a'), 'aw': ('ă','w'), 'ee': ('ê','e'),
    'oo': ('ô','o'), 'ow': ('ơ','w'), 'uw': ('ư','w'), 'dd': ('đ','d'),
}
W_TOGGLE = {'a':'ă','ă':'a','â':'ă','o':'ơ','ơ':'o','u':'ư','ư':'u'}
HAS_LONE_W = True       # 'w' đứng một mình = toggle horn
MODIFIER_PRIORITY = {'a':500,'e':400,'o':300,'w':200,'d':150,
                     's':100,'f':90,'r':80,'x':70,'j':60}
```

### VNISchema (kế thừa TelexSchema)
```python
TONE_KEYS  = {'1':1, '2':2, '3':3, '4':4, '5':5}
MARK_PAIRS = {'a8':('ă','w'), 'a6':('â','a'), 'e6':('ê','e'),
              'o6':('ô','o'), 'o7':('ơ','w'), 'u7':('ư','w'), 'd9':('đ','d')}
W_TOGGLE = {}
HAS_LONE_W = False
```

**Lưu ý:** VNISchema kế thừa toàn bộ hành vi từ TelexSchema; mọi phương thức dùng `cls.TONE_KEYS` / `cls.MARK_PAIRS` nên hoạt động đúng theo polymorphism. `REVERSE_PAIRS` được tự dựng lại qua `__init_subclass__` khi khai báo subclass mới: `char -> [list các pair sinh ra char]`.

## 4. Public API — Đặc tả hành vi

### 4.1 Tra cứu cơ bản

```python
name() -> str                                   # abstract: 'telex' | 'vni'
tone_index(letter) -> int                       # 's'->1, '1'->1; không phải phím thanh -> 0
tone_letter(tone_idx) -> str                    # 1->'s' (telex) / '1' (vni); không có -> ''
is_tone(char) -> bool                           # char có phải phím dấu thanh không (case-insensitive)
raw_pair(char) -> str                           # chuỗi phím đầu tiên sinh ra char; 'â'->'aa' (telex) / 'a6' (vni); không có -> char itself
```

### 4.2 Kiểm tra cặp dấu phụ

```python
is_mark(base, char) -> bool
```
True nếu `char` kết hợp với **bất kỳ ký tự nào** trong `base` (so theo pair `c + char`) tạo thành cặp mark hợp lệ. Vd Telex: `is_mark('traa', 'a')`→True (`a`+`a`); `is_mark('tr', 'a')`→False.

```python
mark_modifier(base, char) -> str
```
Trả **modifier chuẩn** (`'w'/'a'/'e'/'o'/'d'`) của cặp mark khớp, quét `base` **từ phải sang trái** (ưu tiên ký tự gần nhất). Không khớp → `''`. Dùng để Engine biết action đảo/gỡ có cùng ngữ nghĩa giữa Telex và VNI.

### 4.3 Suy luận hành động khi gõ (match với buffer)

```python
match(buffer, key) -> Optional[Action]
```
**API trung tâm cho Engine khi xử lý từng keystroke.** Thứ tự ưu tiên:
1. `key` là phím thanh (`TONE_KEYS`) → trả `ToneAction(index)`. *(Lưu ý: không kiểm buffer.)*
2. `key` là ký tự cuối của một cặp mark, **và** `buffer` kết thúc bằng phần đầu cặp, **và** `Phonology.can_place_mark(buffer, mark_index)` True → trả `MarkAction(index, key)`.
3. Chỉ Telex (`HAS_LONE_W`): `key == 'w'` và có thể đặt horn lên buffer → `MarkAction(3, 'w')` (toggle `u`↔`ư`, `o`↔`ơ`...).
4. Không khớp → `None` (Engine giữ `key` như ký tự thường).

`buffer` là chuỗi chữ hiện tại (chưa áp action); việc so khớp không phân biệt hoa/thường.

### 4.4 Phân tích chuỗi phím tĩnh (rule table)

Module tự sinh **bảng quy tắc** `[(sequence, Action)]` từ `TONE_KEYS` + `MARK_PAIRS`, gồm 3 lớp:
1. Phím thanh: `'s' → ToneAction(1)`.
2. Cặp mark: `'uw' → MarkAction(3,'w')`, `'dd' → MarkAction(4,'d')`.
3. **Quy tắc ghép**: hai cặp cùng modifier áp lên hai nguyên âm kề nhau tạo vần hợp lệ — vd `'uow' → MarkAction(3,'w')` (cả `u` và `o` mang horn → `ươ`). Sinh tự động qua `Phonology.can_place_mark`.

Bảng sắp xếp **dài → ngắn** và được **cache theo class** (`_RULES_CACHE`) — không dựng lại mỗi keystroke.

```python
longest_match(sequence) -> Optional[Action]
```
Action ứng với quy tắc **dài nhất** khớp **phần đầu** của `sequence` (case-insensitive).
- `('uown')` → `MarkAction(3,'w')` (khớp `'uow'`)
- `('as')` → `ToneAction(1)` (khớp `'s'`)
- `('zzz')` → `None`

```python
is_prefix(sequence) -> bool
```
True nếu `sequence` là tiền tố của ít nhất một quy tắc. `('uow')`→True, `('uown')`→False, `('u')`→True, `('')`→False.

```python
longest_prefix(sequence) -> str
```
Phần đầu dài nhất của `sequence` vẫn là tiền tố của một quy tắc (dùng để cắt phần rác). `('uown')`→`'uow'`, `('uowx')`→`'uow'`, `('zzz')`→`''`.

```python
tokens(sequence) -> List[Tuple[str, Optional[Action]]]
```
**Phân tích greedily từ trái sang phải** thành token: `(chuỗi_con, Action)` nếu khớp quy tắc, `(ký_tự_đơn, None)` nếu không.
- `tokens('uown')` → `[('uow', MarkAction(3,'w')), ('n', None)]`
- `tokens('ddsf')` → `[('dd', MarkAction(4,'d')), ('s', ToneAction(1)), ('f', ToneAction(2))]`

## 5. Ghi chú tích hợp (quan trọng cho AI khi dùng module này)

1. **Phân vai module:** `InputSchema` = *dịch phím → Action*; `VietnamesePhonology` = *áp Action lên chữ*. Engine gọi `schema.match(buffer, key)` nhận `Action`, rồi gọi `phonology.place_tone/place_mark` tương ứng. **Không gọi trực tiếp** Phonology cho việc suy luận phím.
2. **Modifier chuẩn (`MarkAction.mod`):** dùng để so sánh giữa schema — vd user đổi từ Telex sang VNI, Engine so cặp action theo `mod` + `index` để biết giữ/gỡ dấu nào. `MODIFIER_PRIORITY` (chỉ Telex) định thứ tự ưu tiên khi có nhiều action chồng nhau trên cùng vị trí (cao → thấp: mark `a,e,o,w,d` rồi tone `s,f,r,x,j`).
3. **`W_TOGGLE`** (chỉ Telex): bảng toggle cho phím `w` độc lập — Engine dùng để đảo `a↔ă`, `o↔ơ`, `u↔ư` (kể cả `â→ă`) khi `match` trả `MarkAction(3,'w')` từ nhánh `HAS_LONE_W`. VNI để bảng rỗng vì không có phím `w` đơn.
4. **Quy tắc ghép `ươ`:** khi `match`/`tokens` trả `MarkAction(3,'w')` khớp chuỗi `'uow'`, Engine phải áp horn lên **cả hai** nguyên âm (dùng `Phonology.place_marks(buffer, [3,3])`), không chỉ một.
5. **Idempotent/rollback:** gõ lại cùng phím trên kết quả đã có dấu thường không tạo action mới (vì `can_place_mark` False hoặc pair không khớp nữa) — Engine cần tự xử lý gỡ dấu (dùng `Phonology.strip_tone/strip_mark` + `TONE_KEYS`/`REVERSE_PAIRS`).
6. **Subclass mới:** chỉ cần override `TONE_KEYS`, `MARK_PAIRS`, `name()`; `REVERSE_PAIRS` tự dựng qua `__init_subclass__`; rule table cache tách riêng theo `cls.__name__`. Đặt `HAS_LONE_W = False` nếu không muốn phím `w` đơn.
7. **Case-insensitive:** mọi so khớp key/sequence đều lowercase trước; `raw_pair`, `tone_letter` trả chuỗi phím dạng chuẩn (lowercase).
8. **Private:** `_rules()` là private (dùng `longest_match`/`is_prefix`/`longest_prefix`/`tokens` thay thế); `match` nhận `self` dù là classmethod-style — gọi qua instance hoặc class đều được.
9. **Dependency:** import path `input_method.vietnamese_phonology` — đảm bảo package layout đúng khi tích hợp; instance Phonology dùng chung là `InputSchema.PHON` (stateless, thread-safe).