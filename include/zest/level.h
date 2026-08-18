#pragma once

#include <cstdint>
#include <optional>
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

// Inverse of `to_string_view`: parse a level name (canonical, lowercase) into a
// `level`. Accepts the "warning" alias for `warn`. Returns `std::nullopt` for
// anything unrecognized, so callers decide the fallback. Useful for reading a
// level from a config file or CLI flag. Match is exact (lowercase); normalize
// case/whitespace before calling if your input source is not.
ZEST_NODISCARD constexpr std::optional<level> from_string_view(std::string_view name) noexcept {
  using namespace std::string_view_literals;
  if (name == "trace"sv) {
    return level::trace;
  }
  if (name == "debug"sv) {
    return level::debug;
  }
  if (name == "info"sv) {
    return level::info;
  }
  if (name == "warn"sv || name == "warning"sv) {
    return level::warn;
  }
  if (name == "error"sv) {
    return level::error;
  }
  if (name == "critical"sv) {
    return level::critical;
  }
  if (name == "off"sv) {
    return level::off;
  }
  return std::nullopt;
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
