#pragma once

#include <cstdint>
#include <string_view>

#include <zest/common.h>

namespace zest {

enum class level : std::uint8_t {
  trace = 0,
  debug = 1,
  info = 2,
  warn = 3,
  error = 4,
  critical = 5,
  off = 6,
  n_levels
};

ZEST_NODISCARD constexpr std::string_view to_string_view(level lvl) noexcept {
  using namespace std::string_view_literals;
  switch (lvl) {
  case level::trace:
    return "trace"sv;
  case level::debug:
    return "debug"sv;
  case level::info:
    return "info"sv;
  case level::warn:
    return "warn"sv;
  case level::error:
    return "error"sv;
  case level::critical:
    return "critical"sv;
  case level::off:
    return "off"sv;
  default:
    return "???"sv;
  }
}

ZEST_NODISCARD constexpr char short_level(level lvl) noexcept {
  switch (lvl) {
  case level::trace:
    return 'T';
  case level::debug:
    return 'D';
  case level::info:
    return 'I';
  case level::warn:
    return 'W';
  case level::error:
    return 'E';
  case level::critical:
    return 'C';
  default:
    return '?';
  }
}

// ---------------------------------------------------------------------------
// Compile-time active level. Log statements below this level are compiled out.
// ---------------------------------------------------------------------------
#define ZEST_LEVEL_TRACE 0
#define ZEST_LEVEL_DEBUG 1
#define ZEST_LEVEL_INFO 2
#define ZEST_LEVEL_WARN 3
#define ZEST_LEVEL_ERROR 4
#define ZEST_LEVEL_CRITICAL 5
#define ZEST_LEVEL_OFF 6

#if !defined(ZEST_ACTIVE_LEVEL)
#define ZEST_ACTIVE_LEVEL ZEST_LEVEL_TRACE
#endif

} // namespace zest
