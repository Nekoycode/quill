// zest — C++20 module interface for the header-only library.
//
// This file is the conventional lib root recognized by the mcpp build tool
// ([targets.zest] kind = "lib" → src/zest.cppm), and is also compiled by CMake
// into the opt-in `zest::module` target (ZEST_BUILD_MODULE=ON). The default
// header-only `zest` INTERFACE target does NOT compile it and is unaffected.
//
// IMPORTANT — macros are NOT exported:
// C++ modules cannot export macros (language limitation). The primary
// ergonomic API — ZEST_INFO(...), ZEST_LOGGER_INFO(logger, ...), and friends,
// gated by ZEST_ACTIVE_LEVEL — is macro-based and therefore available ONLY via
//   #include <zest/zest.h>
// This module exports the function/method API instead: logger::info(...),
// logger::log(source_loc::current(), ...), the sink/logger factories, the
// registry free functions, etc. Everything the macros expand to is reachable
// through the exported function API.
//
// Design: all standard-library and zest headers are included in the global
// module fragment (before `export module zest;`), so their declarations stay
// attached to the global module (no ODR/ABI drift vs. the header-only build).
// The public API is then re-exported name-by-name with `using`-declarations in
// the module purview.

module;

#include <zest/zest.h>

export module zest;

// -- namespace zest ----------------------------------------------------------

export namespace zest {

// Version (common.h).
using zest::version_major;
using zest::version_minor;
using zest::version_patch;
using zest::version_string;

// Levels (level.h).
using zest::from_string_view;
using zest::level;
using zest::short_level;
using zest::to_string_view;

// Core types (source_loc.h, log_msg.h, formatter.h, pattern_formatter.h).
using zest::format_buffer;
using zest::formatter;
using zest::formatter_ptr;
using zest::log_msg;
using zest::pattern_formatter;
using zest::source_loc;

// Loggers (logger.h, async_logger.h).
using zest::async_logger;
using zest::async_msg;
using zest::logger;
using zest::overflow_policy;

// Registry and its free functions (registry.h).
using zest::default_logger;
using zest::drop_all;
using zest::drop_logger;
using zest::flush_all;
using zest::flush_every;
using zest::get_logger;
using zest::registry;
using zest::set_default_logger;
using zest::shutdown;

// Sink factories (zest.h).
using zest::basic_file_sink;
using zest::daily_file_sink;
using zest::json_sink;
using zest::null_sink;
using zest::rolling_file_sink;
using zest::rotating_file_sink;
using zest::stderr_sink;
using zest::stdout_sink;

// Logger factories (zest.h).
using zest::create_async_logger;
using zest::create_logger;
using zest::file_logger;
using zest::file_logger_async;
using zest::stderr_logger;
using zest::stdout_logger;
using zest::stdout_logger_async;

// Default-logger convenience functions (zest.h). These are the module-facing
// equivalents of the ZEST_TRACE..ZEST_CRITICAL macros (which cannot be
// exported): they log to the default logger without call-site source location.
using zest::critical;
using zest::debug;
using zest::error;
using zest::info;
using zest::trace;
using zest::warn;

} // namespace zest

// -- namespace zest::sinks ---------------------------------------------------

export namespace zest::sinks {

using zest::sinks::basic_file_sink;
using zest::sinks::console_sink;
using zest::sinks::daily_file_sink;
using zest::sinks::json_sink;
using zest::sinks::null_sink;
using zest::sinks::rolling_file_sink;
using zest::sinks::rotating_file_sink;
using zest::sinks::sink;

} // namespace zest::sinks
