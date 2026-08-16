#pragma once

#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <quill/common.h>

#if QUILL_USE_STD_FORMAT
  #include <format>
#else
  #include <fmt/format.h>
#endif

namespace quill::detail {

#if QUILL_USE_STD_FORMAT

// Compile-time checked format string. `type_identity_t` keeps `Args...`
// deducible from the argument pack (not from the format string itself), while
// the consteval constructor validates the string against those types.
template <typename... Args>
using format_string_t = std::basic_format_string<char, std::type_identity_t<Args>...>;

template <typename... Args>
inline std::string format(format_string_t<Args...> fmt, Args&&... args) {
  return std::format(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
inline auto make_format_args(Args&&... args) {
  return std::make_format_args(args...);
}

inline std::string vformat(std::string_view fmt, std::format_args args) {
  return std::vformat(fmt, args);
}

#else

template <typename... Args>
using format_string_t = fmt::format_string<Args...>;

template <typename... Args>
inline std::string format(format_string_t<Args...> fmt, Args&&... args) {
  return fmt::format(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
inline auto make_format_args(Args&&... args) {
  return fmt::make_format_args(args...);
}

inline std::string vformat(std::string_view fmt, fmt::format_args args) {
  return fmt::vformat(fmt, args);
}

#endif

} // namespace quill::detail
