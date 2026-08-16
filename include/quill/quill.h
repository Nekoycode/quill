#pragma once

// Public umbrella header. Includes the whole library and defines the logging
// macros plus sink/logger factory functions.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <quill/common.h>
#include <quill/level.h>
#include <quill/source_loc.h>
#include <quill/log_msg.h>
#include <quill/formatter.h>
#include <quill/pattern_formatter.h>
#include <quill/sink.h>
#include <quill/sinks/console_sink.h>
#include <quill/sinks/basic_file_sink.h>
#include <quill/sinks/rotating_file_sink.h>
#include <quill/sinks/daily_file_sink.h>
#include <quill/sinks/null_sink.h>
#include <quill/sinks/json_sink.h>
#include <quill/logger.h>
#include <quill/async_logger.h>
#include <quill/registry.h>

// ---------------------------------------------------------------------------
// Logging macros
//
// The level is passed as a compile-time `quill::level` enumerator and gated at
// the preprocessor level by QUILL_ACTIVE_LEVEL, so disabled statements are
// compiled out entirely (no formatting, no argument evaluation). The source
// location is captured at the call site via std::source_location::current().
// ---------------------------------------------------------------------------
#define QUILL_LOGGER_CALL(logger, lvl, ...)                                                        \
  do {                                                                                             \
    (logger)->log(quill::source_loc::current(), lvl, __VA_ARGS__);                                 \
  } while (false)

#if QUILL_ACTIVE_LEVEL <= QUILL_LEVEL_TRACE
#define QUILL_LOGGER_TRACE(logger, ...) QUILL_LOGGER_CALL(logger, quill::level::trace, __VA_ARGS__)
#else
#define QUILL_LOGGER_TRACE(logger, ...) ((void)0)
#endif

#if QUILL_ACTIVE_LEVEL <= QUILL_LEVEL_DEBUG
#define QUILL_LOGGER_DEBUG(logger, ...) QUILL_LOGGER_CALL(logger, quill::level::debug, __VA_ARGS__)
#else
#define QUILL_LOGGER_DEBUG(logger, ...) ((void)0)
#endif

#if QUILL_ACTIVE_LEVEL <= QUILL_LEVEL_INFO
#define QUILL_LOGGER_INFO(logger, ...) QUILL_LOGGER_CALL(logger, quill::level::info, __VA_ARGS__)
#else
#define QUILL_LOGGER_INFO(logger, ...) ((void)0)
#endif

#if QUILL_ACTIVE_LEVEL <= QUILL_LEVEL_WARN
#define QUILL_LOGGER_WARN(logger, ...) QUILL_LOGGER_CALL(logger, quill::level::warn, __VA_ARGS__)
#else
#define QUILL_LOGGER_WARN(logger, ...) ((void)0)
#endif

#if QUILL_ACTIVE_LEVEL <= QUILL_LEVEL_ERROR
#define QUILL_LOGGER_ERROR(logger, ...) QUILL_LOGGER_CALL(logger, quill::level::error, __VA_ARGS__)
#else
#define QUILL_LOGGER_ERROR(logger, ...) ((void)0)
#endif

#if QUILL_ACTIVE_LEVEL <= QUILL_LEVEL_CRITICAL
#define QUILL_LOGGER_CRITICAL(logger, ...)                                                         \
  QUILL_LOGGER_CALL(logger, quill::level::critical, __VA_ARGS__)
#else
#define QUILL_LOGGER_CRITICAL(logger, ...) ((void)0)
#endif

// Default-logger variants.
#define QUILL_TRACE(...) QUILL_LOGGER_TRACE(quill::default_logger(), __VA_ARGS__)
#define QUILL_DEBUG(...) QUILL_LOGGER_DEBUG(quill::default_logger(), __VA_ARGS__)
#define QUILL_INFO(...) QUILL_LOGGER_INFO(quill::default_logger(), __VA_ARGS__)
#define QUILL_WARN(...) QUILL_LOGGER_WARN(quill::default_logger(), __VA_ARGS__)
#define QUILL_ERROR(...) QUILL_LOGGER_ERROR(quill::default_logger(), __VA_ARGS__)
#define QUILL_CRITICAL(...) QUILL_LOGGER_CRITICAL(quill::default_logger(), __VA_ARGS__)

namespace quill {

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

inline std::shared_ptr<sinks::sink> rotating_file_sink(std::string filename, std::size_t max_size,
                                                       std::size_t max_files) {
  return std::make_shared<sinks::rotating_file_sink>(std::move(filename), max_size, max_files);
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
                                                  Sinks&&... sinks) {
  std::vector<std::shared_ptr<sinks::sink>> v;
  v.reserve(sizeof...(Sinks));
  (v.push_back(std::forward<Sinks>(sinks)), ...);
  auto l = std::make_shared<async_logger>(std::move(name), std::move(v), queue_size);
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
                                                         std::size_t queue_size = 4096) {
  return create_async_logger(name, queue_size, stdout_sink());
}

inline std::shared_ptr<async_logger> file_logger_async(const std::string& name,
                                                       const std::string& filename,
                                                       std::size_t queue_size = 4096) {
  return create_async_logger(name, queue_size, basic_file_sink(filename));
}

} // namespace quill
