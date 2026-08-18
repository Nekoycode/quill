#pragma once

// Public umbrella header. Includes the whole library and defines the logging
// macros plus sink/logger factory functions.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <zest/common.h>
#include <zest/level.h>
#include <zest/source_loc.h>
#include <zest/log_msg.h>
#include <zest/formatter.h>
#include <zest/pattern_formatter.h>
#include <zest/sink.h>
#include <zest/sinks/console_sink.h>
#include <zest/sinks/basic_file_sink.h>
#include <zest/sinks/rotating_file_sink.h>
#include <zest/sinks/daily_file_sink.h>
#include <zest/sinks/null_sink.h>
#include <zest/sinks/json_sink.h>
#include <zest/logger.h>
#include <zest/async_logger.h>
#include <zest/registry.h>

// ---------------------------------------------------------------------------
// Logging macros
//
// The level is passed as a compile-time `zest::level` enumerator and gated at
// the preprocessor level by ZEST_ACTIVE_LEVEL, so disabled statements are
// compiled out entirely (no formatting, no argument evaluation). The source
// location is captured at the call site via std::source_location::current().
// ---------------------------------------------------------------------------
#define ZEST_LOGGER_CALL(logger, lvl, ...)                                                         \
  do {                                                                                             \
    (logger)->log(zest::source_loc::current(), lvl, __VA_ARGS__);                                  \
  } while (false)

#if ZEST_ACTIVE_LEVEL <= ZEST_LEVEL_TRACE
#define ZEST_LOGGER_TRACE(logger, ...) ZEST_LOGGER_CALL(logger, zest::level::trace, __VA_ARGS__)
#else
#define ZEST_LOGGER_TRACE(logger, ...) ((void)0)
#endif

#if ZEST_ACTIVE_LEVEL <= ZEST_LEVEL_DEBUG
#define ZEST_LOGGER_DEBUG(logger, ...) ZEST_LOGGER_CALL(logger, zest::level::debug, __VA_ARGS__)
#else
#define ZEST_LOGGER_DEBUG(logger, ...) ((void)0)
#endif

#if ZEST_ACTIVE_LEVEL <= ZEST_LEVEL_INFO
#define ZEST_LOGGER_INFO(logger, ...) ZEST_LOGGER_CALL(logger, zest::level::info, __VA_ARGS__)
#else
#define ZEST_LOGGER_INFO(logger, ...) ((void)0)
#endif

#if ZEST_ACTIVE_LEVEL <= ZEST_LEVEL_WARN
#define ZEST_LOGGER_WARN(logger, ...) ZEST_LOGGER_CALL(logger, zest::level::warn, __VA_ARGS__)
#else
#define ZEST_LOGGER_WARN(logger, ...) ((void)0)
#endif

#if ZEST_ACTIVE_LEVEL <= ZEST_LEVEL_ERROR
#define ZEST_LOGGER_ERROR(logger, ...) ZEST_LOGGER_CALL(logger, zest::level::error, __VA_ARGS__)
#else
#define ZEST_LOGGER_ERROR(logger, ...) ((void)0)
#endif

#if ZEST_ACTIVE_LEVEL <= ZEST_LEVEL_CRITICAL
#define ZEST_LOGGER_CRITICAL(logger, ...)                                                          \
  ZEST_LOGGER_CALL(logger, zest::level::critical, __VA_ARGS__)
#else
#define ZEST_LOGGER_CRITICAL(logger, ...) ((void)0)
#endif

// Default-logger variants.
#define ZEST_TRACE(...) ZEST_LOGGER_TRACE(zest::default_logger(), __VA_ARGS__)
#define ZEST_DEBUG(...) ZEST_LOGGER_DEBUG(zest::default_logger(), __VA_ARGS__)
#define ZEST_INFO(...) ZEST_LOGGER_INFO(zest::default_logger(), __VA_ARGS__)
#define ZEST_WARN(...) ZEST_LOGGER_WARN(zest::default_logger(), __VA_ARGS__)
#define ZEST_ERROR(...) ZEST_LOGGER_ERROR(zest::default_logger(), __VA_ARGS__)
#define ZEST_CRITICAL(...) ZEST_LOGGER_CRITICAL(zest::default_logger(), __VA_ARGS__)

namespace zest {

// ---------------------------------------------------------------------------
// Default-logger convenience functions
//
// Free-function equivalents of the ZEST_TRACE..ZEST_CRITICAL macros, logging to
// the default logger. Unlike the macros they do not capture the call-site
// source location (they behave exactly like the per-level member functions),
// and they are not gated by ZEST_ACTIVE_LEVEL. They are also the natural entry
// point for consumers of the `zest` C++20 module, which cannot see macros.
// ---------------------------------------------------------------------------

template <typename... Args> void trace(detail::format_string_t<Args...> fmt, Args&&... args) {
  default_logger()->trace(fmt, std::forward<Args>(args)...);
}

template <typename... Args> void debug(detail::format_string_t<Args...> fmt, Args&&... args) {
  default_logger()->debug(fmt, std::forward<Args>(args)...);
}

template <typename... Args> void info(detail::format_string_t<Args...> fmt, Args&&... args) {
  default_logger()->info(fmt, std::forward<Args>(args)...);
}

template <typename... Args> void warn(detail::format_string_t<Args...> fmt, Args&&... args) {
  default_logger()->warn(fmt, std::forward<Args>(args)...);
}

template <typename... Args> void error(detail::format_string_t<Args...> fmt, Args&&... args) {
  default_logger()->error(fmt, std::forward<Args>(args)...);
}

template <typename... Args> void critical(detail::format_string_t<Args...> fmt, Args&&... args) {
  default_logger()->critical(fmt, std::forward<Args>(args)...);
}

// ---------------------------------------------------------------------------
// Sink factories
// ---------------------------------------------------------------------------

inline std::shared_ptr<sinks::sink> stdout_sink() {
  return std::make_shared<sinks::console_sink>(sinks::console_sink::stream::stdout_stream);
}

inline std::shared_ptr<sinks::sink> stderr_sink() {
  return std::make_shared<sinks::console_sink>(sinks::console_sink::stream::stderr_stream);
}

inline std::shared_ptr<sinks::sink> basic_file_sink(std::string filename, bool truncate = false) {
  return std::make_shared<sinks::basic_file_sink>(std::move(filename), truncate);
}

inline std::shared_ptr<sinks::sink> rolling_file_sink(std::string filename, std::size_t max_size,
                                                      std::size_t max_files) {
  return std::make_shared<sinks::rolling_file_sink>(std::move(filename), max_size, max_files);
}

// spdlog-compatible alias.
inline std::shared_ptr<sinks::sink> rotating_file_sink(std::string filename, std::size_t max_size,
                                                       std::size_t max_files) {
  return rolling_file_sink(std::move(filename), max_size, max_files);
}

inline std::shared_ptr<sinks::sink> daily_file_sink(std::string filename, int rotation_hour = 0,
                                                    int rotation_minute = 0) {
  return std::make_shared<sinks::daily_file_sink>(std::move(filename), rotation_hour,
                                                  rotation_minute);
}

inline std::shared_ptr<sinks::sink> null_sink() {
  return std::make_shared<sinks::null_sink>();
}

inline std::shared_ptr<sinks::sink> json_sink(std::string filename, bool truncate = true) {
  return std::make_shared<sinks::json_sink>(std::move(filename), truncate);
}

// ---------------------------------------------------------------------------
// Logger factories
// ---------------------------------------------------------------------------

template <typename... Sinks>
std::shared_ptr<logger> create_logger(std::string name, Sinks&&... sinks) {
  std::vector<std::shared_ptr<sinks::sink>> v;
  v.reserve(sizeof...(Sinks));
  (v.push_back(std::forward<Sinks>(sinks)), ...);
  auto l = std::make_shared<logger>(std::move(name), std::move(v));
  registry::instance().register_logger(l);
  return l;
}

template <typename... Sinks>
std::shared_ptr<async_logger> create_async_logger(std::string name, std::size_t queue_size,
                                                  std::size_t backend_threads, Sinks&&... sinks) {
  std::vector<std::shared_ptr<sinks::sink>> v;
  v.reserve(sizeof...(Sinks));
  (v.push_back(std::forward<Sinks>(sinks)), ...);
  auto l =
      std::make_shared<async_logger>(std::move(name), std::move(v), queue_size, backend_threads);
  registry::instance().register_logger(l);
  return l;
}

// Single-sink convenience factories.
inline std::shared_ptr<logger> stdout_logger(const std::string& name) {
  return create_logger(name, stdout_sink());
}

inline std::shared_ptr<logger> stderr_logger(const std::string& name) {
  return create_logger(name, stderr_sink());
}

inline std::shared_ptr<logger> file_logger(const std::string& name, const std::string& filename,
                                           bool truncate = false) {
  return create_logger(name, basic_file_sink(filename, truncate));
}

inline std::shared_ptr<async_logger> stdout_logger_async(const std::string& name,
                                                         std::size_t queue_size = 4096,
                                                         std::size_t backend_threads = 1) {
  return create_async_logger(name, queue_size, backend_threads, stdout_sink());
}

inline std::shared_ptr<async_logger> file_logger_async(const std::string& name,
                                                       const std::string& filename,
                                                       std::size_t queue_size = 4096,
                                                       std::size_t backend_threads = 1) {
  return create_async_logger(name, queue_size, backend_threads, basic_file_sink(filename));
}

} // namespace zest
