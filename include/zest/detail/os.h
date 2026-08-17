#pragma once

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <thread>

#include <zest/common.h>
#include <zest/level.h>

#ifdef _WIN32
#include <io.h>
#include <process.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace zest::detail {

// Decomposed local time, ready for pattern formatting.
struct tm_fields {
  int year = 0;
  int month = 0; // 1-12
  int day = 0;   // 1-31
  int hour = 0;  // 0-23
  int minute = 0;
  int second = 0;
  int millisecond = 0;
  int microsecond = 0; // 0-999999 (within the second)
  int weekday = 0;     // 0-6, Sunday = 0
};

inline tm_fields to_local_time(std::chrono::system_clock::time_point tp) {
  const std::time_t tt = std::chrono::system_clock::to_time_t(tp);
  std::tm t{};
#ifdef _WIN32
  localtime_s(&t, &tt);
#else
  localtime_r(&tt, &t);
#endif
  const auto micros =
      std::chrono::duration_cast<std::chrono::microseconds>(tp.time_since_epoch()).count() %
      1000000;
  return tm_fields{
      t.tm_year + 1900,
      t.tm_mon + 1,
      t.tm_mday,
      t.tm_hour,
      t.tm_min,
      t.tm_sec,
      static_cast<int>(micros / 1000),
      static_cast<int>(micros),
      t.tm_wday,
  };
}

inline std::uint64_t get_thread_id() {
  return std::hash<std::thread::id>{}(std::this_thread::get_id());
}

inline int process_id() {
#ifdef _WIN32
  return ::_getpid();
#else
  return ::getpid();
#endif
}

inline bool is_tty(std::FILE* f) {
#ifdef _WIN32
  return ::_isatty(::_fileno(f)) != 0;
#else
  return ::isatty(::fileno(f)) != 0;
#endif
}

inline const char* month_short_name(int month) {
  static constexpr const char* names[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  return (month >= 1 && month <= 12) ? names[month - 1] : "?";
}

inline const char* weekday_short_name(int weekday) {
  static constexpr const char* names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  return (weekday >= 0 && weekday <= 6) ? names[weekday] : "?";
}

template <typename S> inline void append_int(S& out, std::int64_t value, int width = 0) {
  char buf[32];
  if (width > 0) {
    std::snprintf(buf, sizeof(buf), "%0*lld", width, static_cast<long long>(value));
  } else {
    std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(value));
  }
  out += buf;
}

template <typename S> inline void append_uint(S& out, std::uint64_t value) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%llu", static_cast<unsigned long long>(value));
  out += buf;
}

inline const char* level_color(level lvl) {
  switch (lvl) {
  case level::trace:
    return "\033[37m";
  case level::debug:
    return "\033[36m";
  case level::info:
    return "\033[32m";
  case level::warn:
    return "\033[33m";
  case level::error:
    return "\033[31m";
  case level::critical:
    return "\033[1;31m";
  default:
    return "\033[0m";
  }
}

inline const char* color_reset() {
  return "\033[0m";
}

#ifdef _WIN32
inline void enable_windows_ansi() {
  const HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
  if (out != INVALID_HANDLE_VALUE && out != nullptr) {
    DWORD mode = 0;
    if (GetConsoleMode(out, &mode) != 0) {
      SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
  }
  const HANDLE err = GetStdHandle(STD_ERROR_HANDLE);
  if (err != INVALID_HANDLE_VALUE && err != nullptr) {
    DWORD mode = 0;
    if (GetConsoleMode(err, &mode) != 0) {
      SetConsoleMode(err, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
  }
}
#endif

} // namespace zest::detail
