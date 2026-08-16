#pragma once

#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include <quill/detail/format.h>

namespace quill::detail {

// Type-erased, owning, unformatted log message with a small inline buffer.
//
// The frontend builds one from a format string + arguments WITHOUT formatting;
// the backend formats it later. Arguments that fit in the inline buffer are
// placement-new'd in place — no heap allocation on the hot path. Larger
// argument packs fall back to a single heap allocation.
class deferred_message {
public:
  static constexpr std::size_t inline_capacity = 64;

  deferred_message() noexcept = default;

  deferred_message(deferred_message&& other) noexcept {
    if (other.move_ != nullptr) {
      other.move_(storage_, other.storage_);
      fmt_ = other.fmt_;
      destroy_ = other.destroy_;
      move_ = other.move_;
      other.reset();
    }
  }

  deferred_message& operator=(deferred_message&& other) noexcept {
    if (this != &other) {
      destroy();
      if (other.move_ != nullptr) {
        other.move_(storage_, other.storage_);
        fmt_ = other.fmt_;
        destroy_ = other.destroy_;
        move_ = other.move_;
        other.reset();
      } else {
        reset();
      }
    }
    return *this;
  }

  ~deferred_message() { destroy(); }

  deferred_message(const deferred_message&) = delete;
  deferred_message& operator=(const deferred_message&) = delete;

  bool empty() const noexcept { return fmt_ == nullptr; }

  void format_into(std::string& out) const { fmt_(storage_, out); }

  template <typename... Args> static deferred_message make(std::string_view fmt, Args&&... args) {
    using holder = holder_t<std::decay_t<Args>...>;
    deferred_message m;

    if constexpr (sizeof(holder) <= inline_capacity &&
                  alignof(holder) <= alignof(std::max_align_t)) {
      new (m.storage_) holder(fmt, std::forward<Args>(args)...);
      m.fmt_ = &format_inline<holder>;
      m.move_ = &move_inline<holder>;
      m.destroy_ = &destroy_inline<holder>;
    } else {
      auto* h = new holder(fmt, std::forward<Args>(args)...);
      std::memcpy(m.storage_, &h, sizeof(h));
      m.fmt_ = &format_heap<holder>;
      m.move_ = &move_heap<holder>;
      m.destroy_ = &destroy_heap<holder>;
    }
    return m;
  }

private:
  template <typename... Args> struct holder_t {
    std::string_view fmt;
    std::tuple<Args...> args;

    holder_t(std::string_view f, Args... a) : fmt(f), args(std::move(a)...) {}
    holder_t(holder_t&&) = default;
    holder_t& operator=(holder_t&&) = default;
    holder_t(const holder_t&) = delete;
    holder_t& operator=(const holder_t&) = delete;

    void format(std::string& out) const {
      out = std::apply(
          [this](const auto&... a) { return detail::vformat(fmt, detail::make_format_args(a...)); },
          args);
    }
  };

  using format_fn = void (*)(const std::byte*, std::string&);
  using move_fn = void (*)(std::byte*, std::byte*);
  using destroy_fn = void (*)(std::byte*);

  template <typename H> static void format_inline(const std::byte* s, std::string& out) {
    reinterpret_cast<const H*>(s)->format(out);
  }

  template <typename H> static void move_inline(std::byte* dst, std::byte* src) {
    H* s = reinterpret_cast<H*>(src);
    new (dst) H(std::move(*s));
    s->~H();
  }

  template <typename H> static void destroy_inline(std::byte* s) { reinterpret_cast<H*>(s)->~H(); }

  template <typename H> static void format_heap(const std::byte* s, std::string& out) {
    H* h;
    std::memcpy(&h, s, sizeof(h));
    h->format(out);
  }

  template <typename H> static void move_heap(std::byte* dst, std::byte* src) {
    H* h;
    std::memcpy(&h, src, sizeof(h));
    std::memcpy(dst, &h, sizeof(h));
    H* null = nullptr;
    std::memcpy(src, &null, sizeof(null));
  }

  template <typename H> static void destroy_heap(std::byte* s) {
    H* h;
    std::memcpy(&h, s, sizeof(h));
    delete h;
  }

  void destroy() {
    if (destroy_ != nullptr) {
      destroy_(storage_);
      destroy_ = nullptr;
    }
  }

  void reset() noexcept {
    fmt_ = nullptr;
    move_ = nullptr;
    destroy_ = nullptr;
  }

  alignas(std::max_align_t) std::byte storage_[inline_capacity];
  format_fn fmt_{nullptr};
  move_fn move_{nullptr};
  destroy_fn destroy_{nullptr};
};

} // namespace quill::detail
