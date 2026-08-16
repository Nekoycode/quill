#pragma once

#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>

namespace quill::detail {

// Small-buffer-optimized string builder. Stores up to `N` bytes inline (no
// heap allocation); overflows to a heap `std::string`. Used to format a log
// line on the stack so the common case never touches the heap.
template <std::size_t N> class small_buffer {
public:
  void push_back(char c) {
    if (!heap_ && size_ < N) {
      inline_[size_++] = c;
    } else {
      overflow().push_back(c);
    }
  }

  small_buffer& operator+=(char c) {
    push_back(c);
    return *this;
  }

  small_buffer& operator+=(const char* s) {
    return s != nullptr ? append_impl(s, std::strlen(s)) : *this;
  }

  small_buffer& operator+=(std::string_view s) { return append_impl(s.data(), s.size()); }

  void append(const char* s, std::size_t n) { append_impl(s, n); }

  const char* data() const noexcept { return heap_ ? heap_->data() : inline_; }
  std::size_t size() const noexcept { return heap_ ? heap_->size() : size_; }
  std::string_view view() const noexcept { return std::string_view(data(), size()); }

private:
  small_buffer& append_impl(const char* s, std::size_t n) {
    if (!heap_ && size_ + n <= N) {
      std::memcpy(inline_ + size_, s, n);
      size_ += n;
    } else {
      overflow().append(s, n);
    }
    return *this;
  }

  std::string& overflow() {
    if (!heap_) {
      heap_ = std::make_unique<std::string>(inline_, size_);
    }
    return *heap_;
  }

  char inline_[N]{};
  std::size_t size_{0};
  std::unique_ptr<std::string> heap_;
};

} // namespace quill::detail
