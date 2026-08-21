#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace zest::detail {

// Fixed-capacity ring buffer that overwrites the oldest entry when full.
// `capacity` must be > 0 (the modulo indexing divides by it); a 0 is clamped
// to 1. Callers (logger backtrace) already reject a 0 capacity.
template <typename T> class circular_buffer {
public:
  explicit circular_buffer(std::size_t capacity)
      : buf_(capacity == 0 ? 1 : capacity), capacity_(capacity == 0 ? 1 : capacity) {}

  void push(const T& value) {
    buf_[(head_ + size_) % capacity_] = value;
    if (size_ < capacity_) {
      ++size_;
    } else {
      head_ = (head_ + 1) % capacity_;
    }
  }

  std::vector<T> snapshot() const {
    std::vector<T> out;
    out.reserve(size_);
    for (std::size_t i = 0; i < size_; ++i) {
      out.push_back(buf_[(head_ + i) % capacity_]);
    }
    return out;
  }

  std::size_t size() const noexcept { return size_; }

private:
  std::vector<T> buf_;
  std::size_t head_{0};
  std::size_t size_{0};
  std::size_t capacity_;
};

} // namespace zest::detail
