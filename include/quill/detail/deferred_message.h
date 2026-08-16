#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include <quill/detail/format.h>

namespace quill::detail {

// Type-erased, owning, unformatted log message. The frontend builds one from a
// format string + arguments WITHOUT formatting; the backend formats it later.
// This deferred formatting is the key architectural difference from spdlog,
// whose async logger formats on the calling thread.
class deferred_message {
public:
  virtual ~deferred_message() = default;
  virtual void format_into(std::string& out) const = 0;
};

using deferred_message_ptr = std::unique_ptr<deferred_message>;

template <typename... Args> class deferred_message_impl final : public deferred_message {
public:
  deferred_message_impl(std::string_view fmt, Args... args)
      : fmt_(fmt), args_(std::move(args)...) {}

  void format_into(std::string& out) const override {
    out = std::apply(
        [this](const auto&... a) { return detail::vformat(fmt_, detail::make_format_args(a...)); },
        args_);
  }

private:
  std::string fmt_;
  std::tuple<Args...> args_;
};

template <typename... Args>
deferred_message_ptr make_deferred_message(std::string_view fmt, Args&&... args) {
  return std::make_unique<deferred_message_impl<std::decay_t<Args>...>>(
      fmt, std::forward<Args>(args)...);
}

} // namespace quill::detail
