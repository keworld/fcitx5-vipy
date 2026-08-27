#ifndef VICPLEX_CHAR_BUFFER_HPP
#define VICPLEX_CHAR_BUFFER_HPP

#include <array>
#include <cstddef>
#include <cstring>

namespace vipy {

    constexpr size_t kMaxWordLength = 32;

    class CharBuffer {
    public:
        void clear() { len_ = 0; vniRawLen_ = 0; }
        [[nodiscard]] bool empty() const { return len_ == 0; }
        [[nodiscard]] size_t size() const { return len_; }
        [[nodiscard]] bool full() const { return len_ >= kMaxWordLength; }

        void push(char32_t cp) {
            if (len_ < kMaxWordLength) data_[len_++] = cp;
        }

        void pop() {
            if (len_ > 0) --len_;
        }

        [[nodiscard]] char32_t back() const { return data_[len_ - 1]; }
        char32_t operator[](size_t i) const { return data_[i]; }
        char32_t &operator[](size_t i) { return data_[i]; }

        [[nodiscard]] const char32_t *data() const { return data_.data(); }

        void assignFrom(const CharBuffer &o) noexcept {
            len_ = o.len_;
            std::memcpy(data_.data(), o.data_.data(), len_ * sizeof(char32_t));
            vniRawLen_ = o.vniRawLen_;
            std::memcpy(vniRaw_.data(), o.vniRaw_.data(), vniRawLen_);
        }

        void assignContentFrom(const CharBuffer &o) noexcept {
            len_ = o.len_;
            std::memcpy(data_.data(), o.data_.data(), len_ * sizeof(char32_t));
        }

        [[nodiscard]] size_t vniRawSize() const { return vniRawLen_; }
        [[nodiscard]] char vniRawAt(size_t i) const { return vniRaw_[i]; }
        void pushVniRaw(char c) {
            if (vniRawLen_ < kMaxWordLength) vniRaw_[vniRawLen_++] = c;
        }
        void popVniRaw() {
            if (vniRawLen_ > 0) --vniRawLen_;
        }
        void eraseVniRaw(size_t index) {
            if (index >= vniRawLen_) return;
            for (size_t i = index + 1; i < vniRawLen_; ++i) vniRaw_[i - 1] = vniRaw_[i];
            --vniRawLen_;
        }
        void clearContent() { len_ = 0; }

    private:
        std::array<char32_t, kMaxWordLength> data_{};
        size_t len_ = 0;
        std::array<char, kMaxWordLength> vniRaw_{};
        size_t vniRawLen_ = 0;
    };

} // namespace vipy

#endif // VIPY_CHAR_BUFFER_HPP