# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- mcpp build-tool entry (`mcpp.toml`) as an alternative to the CMake flow.
- C++20 module interface `src/zest.cppm` (`import zest;`) re-exporting the full
  function/method API. Macros (`ZEST_INFO`, …) cannot be exported by modules and
  remain header-only (`#include <zest/zest.h>`); the exported free functions and
  member functions cover what the macros expand to. Built automatically by mcpp,
  and by CMake via the opt-in `ZEST_BUILD_MODULE` target `zest::module`
  (CMake >= 3.28, Ninja, gcc >= 16 / clang >= 17 — gcc 15 hits GCC bug 101140).
- `zest::level::from_string_view` — parse a level name (`"info"`, `"warning"`,
  …) into `zest::level`, for config/CLI-driven level selection.
- `zest::version_string` — a `std::string_view` version constant (reachable from
  the C++20 module, unlike the `ZEST_VERSION_STRING` macro).
- Default-logger convenience free functions
  `zest::trace`/`debug`/`info`/`warn`/`error`/`critical(fmt, args…)` — the
  function equivalents of the `ZEST_*` macros.
- A native `{field}` brace pattern syntax (`{datetime} [{level}] {logger}:
  {msg}`, …) alongside the spdlog-compatible `%`-flags — distinct from both
  spdlog's `%` and quill's `%(named)`; `{{`/`}}` escape literal braces and the
  two syntaxes mix freely in one pattern.
- Async **overflow policy** (`zest::overflow_policy::block` / `drop_oldest` /
  `drop_newest`) for the bounded queue — what to do when the queue is full
  instead of always blocking the producer.
- `zest::flush_every(interval)` — flush all loggers periodically on a background
  thread (stops on a non-positive interval, `shutdown()`, or program exit), so a
  crash loses at most one interval of buffered output.
- README mcpp build documentation, including first-build toolchain download
  behavior and China-network notes (gitcode CDN vs. GitHub release assets).

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
