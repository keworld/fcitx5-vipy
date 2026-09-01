# Module: `vietnamese_phonology.py` — Tài liệu Đặc tả cho AI

## 1. Mục đích Module

Module cung cấp lớp `VietnamesePhonology xử lý **âm vị học tiếng Việt** ở mức âm tiết (syllable): nhận diện phụ âm/nguyên âm, tách và đặt **dấu phụ (mark)** — breve `ă`, circumflex `â ê ô`, horn `ơ ư`, stroke `đ` — và **dấu thanh (tone)** — sắc `´`, huyền `` ` ``, hỏi `?`, ngã `~`, nặng `.` — theo đúng quy tắc chính tả tiếng Việt.

**Use case chính:** xây dựng bộ gõ tiếng Việt (input method engine), kiểm tra tính hợp lệ của âm tiết đang gõ, tiền xử lý văn bản tiếng Việt.

**Yêu cầu:** Python 3.7+, dùng `unicodedata` (chuẩn hóa NFC/NFD) và `dataclasses`.

## 2. Mô hình dữ liệu

| Dataclass | Trường | Ý nghĩa |
|---|---|---|
| `Diacritic` | `name`, `char`, `base` | mark phụ: ví dụ `('horn', 'ơ', 'o')` |
| `Tone` | `name`, `index`, `combining` | thanh điệu: `index` 1–5 tương ứng sắc→nặng; `combining` là codepoint Unicode (U+0301, U+0300, U+0309, U+0303, U+0323) |
| `Mark` | `index`, `name`, `on_vowel` | index 1=breve, 2=circumflex, 3=horn, 4=stroke (`on_vowel=False`) |

## 3. Từ vựng đặc tả (AI phải hiểu đúng các thuật ngữ)

- **tone**: dấu thanh (1–5). `0` = không có thanh.
- **mark**: dấu phụ trên nguyên âm (breve/circumflex/horn) hoặc biến đổi phụ âm (stroke `d`→`đ`).
- **bare form**: chuỗi đã bỏ **mọi** dấu combining và đổi `đ`→`d` (viết thường).
- **onset**: phụ âm đầu (single/double/triple, ví dụ `ngh`).
- **nucleus**: cụm nguyên âm chính của vần.
- **final (coda)**: phụ âm cuối (`c m n p t` / `ch nh ng`).
- **vowel_components**: các ký tự nguyên âm đơn theo thứ tự trong chuỗi, vd `'ươ'` → `['u','o']`.

## 4. Public API — Đặc tả hành vi

### 4.1 Nhận diện

```python
is_consonant(string) -> bool
```
True nếu `string` (lowercase) là onset **hoặc** final hợp lệ. Ví dụ: `'ng'`→True, `'đ'`→True, `'a'`→False.

```python
is_vowel(string) -> bool
```
- Nếu chuỗi **có** dấu phụ (ă â ê ô ơ ư): so khớp chính xác với bảng `VOWELS` (`'âi'` ≠ `'ai'`).
- Nếu **không dấu**: so sánh dạng bare, cho phép khớp nhiều biến thể — `'uo'`→True (khớp `uô`/`ươ`). Phục vụ kiểm tra khi đang gõ.

```python
has_vowel(string) -> bool
```
True nếu chuỗi chứa ít nhất một nguyên âm (kể cả có dấu).

### 4.2 Kiểm tra tiền tố (dùng khi đang gõ — prefix matching)

```python
is_onset_prefix(string) -> bool
```
True nếu `string` là prefix (kể cả chưa hoàn chỉnh) của một onset hợp lệ. `''` → False. `'n'`, `'ng'`, `'ngh'` đều True.

```python
is_nucleus_prefix(string) -> bool
```
True nếu bare form của `string` là prefix của một nucleus hợp lệ. `'uo'`→True (có thể thành `uô`/`ươ`), `'uô'`→True.

```python
is_rhyme_prefix(string) -> bool
```
True nếu `string` có thể là tiền tố của một vần (nucleus + final). Kiểm: (a) là nucleus-prefix, hoặc (b) tồn tại điểm cắt sao cho phần nguyên âm hợp lệ **và** phần final là prefix của final hợp lệ.

```python
is_valid_shape(string) -> bool
```
True nếu `string` là **âm tiết hoàn chỉnh**: sau khi strip tone, tách onset–nucleus–final, nucleus phải nằm đúng trong `VOWELS`, và **nucleus triple (iêu, ươu, uây…) không được mang final**.

```python
can_grow(string) -> bool
```
True nếu `string` có thể là **tiền tố của một từ hợp lệ** (dùng để quyết định cho phép tiếp tục gõ). Chuỗi rỗng → True.

### 4.3 Trích xuất trạng thái

```python
strip_tone(string) -> str        # bỏ chỉ dấu thanh, giữ dấu phụ và đ
strip_mark(string) -> str        # bỏ chỉ dấu phụ, giữ dấu thanh
bare(string) -> str              # bỏ tất cả dấu + đ→d, lowercase
word_tone(string) -> int         # trả index thanh 1–5, 0 nếu không có
word_mark(string) -> int         # trả index mark 1–4, 0 nếu không có
vowel_components(string) -> List[str]  # vd 'qủa' -> ['u','a']
```

### 4.4 Đặt dấu thanh (tone)

```python
place_tone(string, tone: int) -> str
```
Đặt dấu thanh tại vị trí **đúng quy tắc tiếng Việt**, tự động xử lý:
- `qu`/`gi` đầu từ: dấu đặt vào nguyên âm **sau** chúng (`'qua'` + huyền → `quà`, `'gi'`+sắc → `gí`).
- Cụm `ươ`/`uơ`: dấu vào `ơ`; cụm `ưa`: dấu vào `ư`.
- Nguyên âm đã có dấu phụ: dấu thanh chồng lên chính chữ đó (`'â'`+sắc → `ấ`).
- Vần đóng (có coda): dấu vào nguyên âm cuối.
- Vần mở: dấu vào nguyên âm áp chót; ngoại lệ `oa`, `oe`, `uy` → dấu vào âm đầu (`'oa'`+sắc → `óa`).
- `tone` không hợp lệ (0 hoặc >5) → trả chuỗi gốc. **Bảo toàn hoa/thường** (`'Viet'`→ huyền: `'Việt'`, `'Đ'` giữ nguyên dạng hoa).

### 4.5 Đặt dấu phụ (mark) — đơn

```python
place_mark(string, mark: int) -> str
```
- `mark=1` breve: `'a'`→`'ă'` (chữ nguyên âm đầu tiên áp dụng được).
- `mark=2` circumflex: `'a/e/o'`→`'â/ê/ô'`.
- `mark=3` horn: `'o'`→`'ơ'`, `'u'`→`'ư'`; **quy tắc đặc biệt**: `'uo'` ở **cuối** chuỗi → chỉ `'o'` mang horn (`'uo'`→`'uơ'`); `'uo'` có ký tự theo sau → **cả hai** mang horn (`'uoc'`→`'ươc'`).
- `mark=4` stroke: chỉ đổi `'d'` **đầu từ** → `'đ'`.
- Tự **giữ lại dấu thanh** đang có trên chuỗi (đặt lại đúng vị trí sau biến đổi).
- Không áp dụng được → trả chuỗi gốc. Bảo toàn hoa/thường.

```python
can_place_mark(string, mark_index) -> bool
```
True nếu đặt mark cho ra **vần hợp lệ**:
- mark=4 (stroke): chỉ cần có biến đổi `d`→`đ` xảy ra.
- mark nguyên âm: cụm nguyên âm kết quả (dạng **có dấu**, so trực tiếp không bare — `uă` ≠ `ua`) phải nằm trong `VOWELS`.

```python
mark_position(string, mark_index) -> int
```
Trả **index ký tự** trong `string` nơi mark sẽ được đặt; `-1` nếu không hợp lệ. Vd: `('uo',3)`→1, `('uoc',3)`→0, `('aa',3)`→-1.

### 4.6 Đặt dấu phụ (mark) — đồng thời nhiều mark

```python
place_marks(string, mark_indices: List[int]) -> str
```
Áp mark cho **từng thành phần nguyên âm** theo thứ tự của `vowel_components`. `0` = bỏ qua. Độ dài list phải bằng số nguyên âm, nếu không trả chuỗi gốc. Giữ tone hiện có, bảo toàn hoa/thường.

Ví dụ: `('uo',[3,3])`→`'ươ'`; `('uye',[0,0,2])`→`'uyê'`; `('oa',[1,0])`→`'oă'`.

```python
can_combine_marks(string, mark_indices: List[int]) -> bool
```
True nếu kết quả đặt đồng thời là vần hợp lệ. Vd: `('uo',[3,3])`→True; `('ea',[2,2])`→False (`êâ` không tồn tại). Tất cả `0` → True (không đổi).

## 5. Bảng dữ liệu chuẩn (tham chiếu)

- **Onset single:** `b c d đ g h k l m n p q r s t v x`
- **Onset double:** `ch gh gi kh nh ng ph qu th tr`
- **Onset triple:** `ngh`
- **Final single:** `c m n p t` — **Final double:** `ch nh ng`
- **Vowel single:** `a ă â e ê i o ô ơ u ư y`
- **Vowel double:** `ai ao au ay âu ây eo ia iê iu oa oă oe oi ôi ơi ua uâ uơ uô ui uy ưa ươ ưi ưu uê yê êu`
- **Vowel triple:** `iêu oai oao oay oeo uai uay uây uôi uyu uyê ươu yêu`

## 6. Ghi chú tích hợp (quan trọng cho AI khi dùng module này)

1. **Chuẩn hóa Unicode:** mọi kết quả trả về ở dạng **NFC**; đầu vào nên chuẩn hóa NFC trước khi gọi.
2. **Vị trí dấu thanh** được tính theo nguyên tắc: quét onset (3→2→1 ký tự) rồi final (2→1 ký tự) trên dạng **bare**; vì NFC mỗi ký tự có dấu = 1 codepoint nên index giữa bare và signed khớp nhau.
3. **Không dùng** `_valid_tone_index`, `_transform_tone`, `_transform_mark`, `_bare_vowel`, `_restore_case`, `_is_onset`, `_is_final`, `_vowel_of` — đây là private.
4. **Pattern gõ điển hình:** khi người dùng gõ từng ký tự, gọi `can_grow()` để chặn chuỗi không thể thành từ; khi nhận phím mark/tone, gọi `can_place_mark`/`can_combine_marks` để quyết định áp dụng; dùng `place_mark`/`place_marks`/`place_tone` để biến đổi; dùng `word_tone`/`strip_tone`/`strip_mark`/`bare` để trích trạng thái phục vụ undo hoặc chuyển kiểu gõ (VNI/Telex).
5. **Case-preserving:** tất cả API biến đổi (`place_tone`, `place_mark`, `place_marks`) tự khôi phục chữ hoa theo vị trí gốc; `đ` ↔ `Đ` được xử lý đúng.
6. Lớp là **stateless** (chỉ có hằng số lớp + phương thức), an toàn chia sẻ một instance giữa nhiều luồng xử lý.