# Tài liệu Đặc tả (Instruction): Module `InputSchema`

## 1. Tổng quan (Overview)
Module `InputSchema` cung cấp bộ khung (framework) để phân tích và xử lý logic gõ tiếng Việt. Nó chịu trách nhiệm nhận diện các phím gõ đầu vào (`key`) áp dụng lên một chuỗi gốc (`base_string`) để xác định hành động cần thực hiện (thêm/xóa dấu thanh, thêm/xóa dấu chữ). Module được thiết kế linh hoạt để hỗ trợ nhiều kiểu gõ khác nhau thông qua tính kế thừa (hiện tại hỗ trợ Telex và VNI).

## 2. Cấu trúc dữ liệu
### `Action` (Dataclass)
Đại diện cho kết quả của một thao tác gõ.
* **`value`** (`int`): Giá trị của dấu thanh (1-5) hoặc dấu chữ (1-4). Mặc định: `0` (nếu không có hành động).
* **`type`** (`str`): Loại hành động. Các giá trị hợp lệ:
    * `'none'`: Không có hành động nào khớp.
    * `'mark'`: Đặt dấu chữ (ví dụ: a -> â, d -> đ).
    * `'tone'`: Đặt dấu thanh (sắc, huyền, hỏi, ngã, nặng).
    * `'toggle_mark'`: Hủy dấu chữ khi gõ lặp lại phím tạo dấu.
    * `'toggle_tone'`: Hủy dấu thanh khi gõ lặp lại phím tạo thanh.

## 3. Lớp cơ sở `InputSchema` (Abstract Base Class)
Lớp trừu tượng định nghĩa giao diện và logic cốt lõi cho mọi kiểu gõ.

### 3.1. Thuộc tính lớp (Class Attributes)
* **`PHON`**: Thể hiện (`instance`) của `VietnamesePhonology`, cung cấp các hàm xử lý ngữ âm cốt lõi (kiểm tra nguyên âm, vị trí đặt dấu, v.v.).
* **`TONE_KEYS`** (`dict`): Ánh xạ ký tự gõ sang giá trị dấu thanh (1-5).
* **`MARK_KEYS`** (`tuple`): Tập hợp các ký tự được sử dụng để tạo dấu chữ.
* **`MARK_PAIRS`** (`dict`): Ánh xạ tổ hợp phím (ví dụ: 'aa', 'aw', 'a8') sang giá trị dấu chữ (1-4).

### 3.2. Phương thức (Methods)
* **`name(cls) -> str`** *(Abstract)*: Trả về định danh của kiểu gõ (VD: `'telex'`, `'vni'`).
* **`is_mark(cls, key: str) -> int`**: Kiểm tra một phím có phải là phím tạo dấu chữ hay không. 
    * *Return:* Giá trị dấu (1-4) hoặc `-1` (không hợp lệ).
* **`is_tone(cls, key: str) -> int`**: Kiểm tra một phím có phải là phím tạo dấu thanh hay không.
    * *Return:* Giá trị dấu thanh (1-5) hoặc `-1` (không hợp lệ).
* **`match(cls, base_string: str, key: str) -> Action`**: Hàm xử lý logic chính, đánh giá tác động của `key` lên `base_string`.

## 4. Luồng xử lý logic của `match(base_string, key)`
Phương thức này hoạt động theo trình tự sau:

**Tiền xử lý:** Chuyển `key` về in thường. Nếu `base_string` rỗng, trả về `Action(0, 'none')`.

### Bước 1: Kiểm tra Dấu thanh (Tone)
Nếu `key` nằm trong `TONE_KEYS`:
1. Lấy giá trị thanh (`tone`) tương ứng. Trích xuất thanh hiện tại (`base_tone`) của chuỗi.
2. **Hủy thanh (Toggle Tone):** Nếu `tone == base_tone`, trả về `Action(value=tone, type='toggle_tone')`.
3. **Đặt thanh (Place Tone):** Nếu chuỗi có nguyên âm và thỏa mãn điều kiện thêm dấu của thuật toán ngữ âm (`can_grow`), thử đặt dấu. Nếu chuỗi thay đổi, trả về `Action(value=tone, type='tone')`.
4. *Fallback:* Trả về `Action(0, 'none')`.

### Bước 2: Kiểm tra Dấu chữ (Mark)
Nếu `key` nằm trong `MARK_KEYS`:
1. Tách bỏ thanh âm để lấy chuỗi trần (`bare`) và danh sách các dấu chữ hiện có (`base_marks`).
2. **Xử lý chữ 'đ' (Stroke):**
   - Kiểm tra chuỗi có bắt đầu/kết thúc bằng 'd', 'D', 'đ', hoặc 'Đ'.
   - Nếu tổ hợp (ví dụ 'd' + key) khớp trong `MARK_PAIRS` (thường sinh ra mark `4`):
     - Nếu mark đã tồn tại -> Trả về `Action(type='toggle_mark')`.
     - Nếu có thể đặt mark -> Trả về `Action(value=4, type='mark')`.
3. **Xử lý nguyên âm (Vowels):**
   - Trích xuất các nguyên âm trong chuỗi gốc, duyệt ngược từ cuối lên đầu.
   - Kiểm tra tổ hợp `ch + key` trong `MARK_PAIRS`.
   - Nếu tồn tại `mark`:
     - Nếu `mark` đang có sẵn trên chuỗi -> Trả về `Action(type='toggle_mark')`.
     - Nếu vị trí đặt hợp lệ hoặc cho phép đặt -> Trả về `Action(value=mark, type='mark')`.

**Kết quả mặc định:** Nếu không khớp bất kỳ logic nào, trả về `Action(0, 'none')`.

## 5. Các lớp triển khai (Implementations)

### `TelexSchema`
Kiểu gõ Telex truyền thống.
* **Tones:** `'s'` (1), `'f'` (2), `'r'` (3), `'x'` (4), `'j'` (5).
* **Marks:** `'w'`, `'a'`, `'e'`, `'o'`, `'d'`.
* **Pairs:** 
  - `'aa'` -> 2 (â)
  - `'aw'` -> 1 (ă)
  - `'ee'` -> 2 (ê)
  - `'oo'` -> 2 (ô)
  - `'ow'` -> 3 (ơ)
  - `'uw'` -> 3 (ư)
  - `'dd'` -> 4 (đ)

### `VNISchema`
Kiểu gõ VNI sử dụng phím số.
* **Tones:** `'1'` (1), `'2'` (2), `'3'` (3), `'4'` (4), `'5'` (5).
* **Marks:** `'6'`, `'7'`, `'8'`, `'9'`.
* **Pairs:** 
  - `'a8'` -> 1 (ă)
  - `'a6'` -> 2 (â)
  - `'e6'` -> 2 (ê)
  - `'o6'` -> 2 (ô)
  - `'o7'` -> 3 (ơ)
  - `'u7'` -> 3 (ư)
  - `'d9'` -> 4 (đ)
