# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.1.0] - 2026-08-16

### Added

- Header-only C++20 logging library with zero mandatory dependencies
  (`std::format`, `std::chrono`, `std::source_location`, `std::jthread`).
- spdlog-like API: `logger`, `sinks`, `pattern_formatter`, `level`, `registry`,
  and the `ZEST_LOGGER_*`/`ZEST_*` logging macros.
- Compile-time format string checking (`std::format_string`) and compile-time
  level gating (`ZEST_ACTIVE_LEVEL`); `log_runtime`/`log_raw` escape hatches.
- Sinks: `console_sink` (auto ANSI color), `basic_file_sink`, `rolling_file_sink`
  (size-based rotation, alias `rotating_file_sink`), `daily_file_sink`,
  `null_sink`, `json_sink` (structured JSON records).
- Backtrace (`enable_backtrace` / `disable_backtrace` / `dump_backtrace`).
- Async logger with deferred formatting: allocation-free frontend
  (small-buffer-optimized argument capture), bounded lock-free MPMC queue, and a
  configurable backend thread pool; graceful shutdown drains pending records.
- CMake presets (`dev`/`debug`/`release`/`relwithdebinfo`/`bench`/`ci`/
  `asan`/`tsan`/`coverage`) and install/export (`find_package(zest CONFIG)`).
- GitHub Actions CI: 3-platform build/test matrix, sanitizers (ASan/UBSan/TSan),
  fmt fallback, and `check-format`/`clang-tidy`/`coverage` quality gates.
- Benchmarks (std::chrono micro-benchmarks and an optional spdlog comparison).
- `std::format` by default with an opt-in `fmt` fallback (`ZEST_USE_STD_FORMAT`).

[Unreleased]: https://github.com/Nekoycode/zest/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/Nekoycode/zest/releases/tag/v0.1.0
