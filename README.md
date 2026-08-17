# zest

[English](README.md) · [简体中文](README.zh-CN.md)

A lightweight, highly-customizable and thread-safe C++20 logging library.

`zest` combines an **spdlog-like API** (logger → sinks → pattern formatter →
level → registry) with a **frontend/backend async engine**: the hot path only
captures arguments (allocation-free), while a background thread pool formats
and performs the I/O. This deferred formatting makes the async path ~3× faster
than spdlog's frontend-formatting async logger.

---

## Highlights

- **Header-only, zero mandatory dependencies** — built on the C++20 standard
  library (`std::format`, `std::chrono`, `std::source_location`,
  `std::jthread`, `std::stop_token`).
- **spdlog-like API** — familiar `logger`, `sinks`, pattern formatter, level
  filtering and a global registry.
- **Reliable concurrency** — a bounded lock-free MPMC queue feeding a
  configurable backend thread pool; graceful shutdown drains pending records.
- **Allocation-free async hot path** — arguments are captured into a
  small-buffer-optimized holder (no heap allocation for the common case).
- **Compile-time safety** — format strings are checked at compile time, and
  `ZEST_ACTIVE_LEVEL` removes disabled statements entirely.
- **Structured logging** — a `json_sink` emits one JSON object per record.
- **Backtrace** — keep the last N records and replay them after an error.
- **Modern CMake** — target-based build, CMake presets, and install/export
  with `find_package(zest CONFIG)`.

## Requirements

- C++20 compiler: GCC ≥ 13, Clang ≥ 15, MSVC ≥ 19.29.
- CMake ≥ 3.25 and [Ninja](https://ninja-build.org/) (recommended generator).

## Quick start

```cpp
#include <zest/zest.h>

int main() {
  auto logger = zest::stdout_logger("app");
  logger->set_pattern("%^[%H:%M:%S.%e] [%l] [%n] %v%$");

  ZEST_INFO(logger, "hello {}!", "world");
  ZEST_WARN(logger, "the answer is {}", 42);
  return 0;
}
```

```bash
cmake --preset dev
cmake --build --preset dev
./build/dev/examples/zest_basic
```

## Logging

### Macros

The `ZEST_LOGGER_*` macros take a logger; the `ZEST_*` macros use the default
logger (created lazily):

```cpp
ZEST_LOGGER_TRACE(logger, "trace");      // named logger
ZEST_LOGGER_DEBUG(logger, "x = {}", 1);
ZEST_LOGGER_INFO(logger, "info");
ZEST_LOGGER_WARN(logger, "warn");
ZEST_LOGGER_ERROR(logger, "error");
ZEST_LOGGER_CRITICAL(logger, "critical");

ZEST_INFO("default logger");             // default logger
```

The source location is captured at the call site via
`std::source_location::current()`.

### Levels and filtering

```cpp
logger->set_level(zest::level::warn);   // runtime filter (per logger)
```

To compile out a level entirely, define `ZEST_ACTIVE_LEVEL` before including
zest (e.g. `-DZEST_ACTIVE_LEVEL=ZEST_LEVEL_WARN`); statements below that
level are removed at the preprocessor stage.

### Pattern formatting

Configure the output with `set_pattern`. Supported flags:

| Flag | Meaning | Flag | Meaning |
|---|---|---|---|
| `%Y` `%m` `%d` | year / month / day | `%H` `%M` `%S` | hour / minute / second |
| `%b` `%a` | month / weekday short name | `%e` `%f` | milliseconds / microseconds |
| `%l` `%L` | level short / full | `%n` | logger name |
| `%t` `%P` | thread id / process id | `%v` | message text |
| `%s` | source `file:line` | `%g` `%#` `%!` | file / line / function |
| `%^` `%$` | start / end color | `%%` | literal `%` |

### Runtime format strings

`log`/`trace`/... use compile-time-checked format strings. For runtime strings
use `log_runtime` (formats with `std::vformat`) or `log_raw` (pre-formatted
message).

## Sinks

| Sink | Description |
|---|---|
| `console_sink` | stdout/stderr, automatic ANSI color (TTY-aware) |
| `basic_file_sink` | append to a single file (optionally truncate) |
| `rolling_file_sink` | size-based rolling; keeps `max_files` backups (`rotating_file_sink` alias) |
| `daily_file_sink` | one file per day, rolls at a configurable time |
| `null_sink` | discards everything (benchmarks) |
| `json_sink` | one JSON object per record |

```cpp
// one sink
auto logger = zest::file_logger("app", "app.log");

// several sinks
auto logger = zest::create_logger(
    "multi", zest::stdout_sink(),
    zest::rolling_file_sink("app.log", /*max_size=*/1024 * 1024,
                             /*max_files=*/5));
```

### Custom sinks

Subclass `zest::sinks::sink` and implement `flush()` plus the protected
`write_output(std::string_view)` (or override `write()` for structured output):

```cpp
class my_sink final : public zest::sinks::sink {
public:
  void flush() override {}

protected:
  void write_output(std::string_view line) override {
    std::fwrite(line.data(), 1, line.size(), stdout);
  }
};
```

## Async logging

```cpp
// queue size + number of backend threads
auto logger =
    zest::create_async_logger("async", /*queue_size=*/65536,
                               /*backend_threads=*/4, zest::basic_file_sink("app.log"));

ZEST_LOGGER_INFO(logger, "this formats and writes on a background thread");
logger->flush();
```

The frontend captures arguments without formatting (allocation-free for the
common case); backend threads format and write. With more than one backend
thread, records are not strictly FIFO ordered. Shutdown drains all pending
records.

## Backtrace

```cpp
logger->enable_backtrace(32);
// ... log normally ...
logger->dump_backtrace();   // replay the last 32 records
logger->disable_backtrace();
```

## JSON logging

```cpp
auto logger = zest::create_logger("json", zest::json_sink("app.json"));
ZEST_LOGGER_INFO(logger, "user {} logged in", "alice");
// {"time":"2026-08-16T10:30:00.123","level":"info","logger":"json",...}
```

## Building

```bash
cmake --preset dev            # configure (Debug + tests + examples + -Werror)
cmake --build --preset dev
ctest  --preset dev           # run tests
```

Configure presets: `dev`, `debug`, `release`, `relwithdebinfo`, `bench`, `ci`,
`asan`, `tsan`, `coverage`.

## Consuming from another CMake project

```cmake
find_package(zest CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE zest::zest)
```

Or via FetchContent / `add_subdirectory` — `zest::zest` is an INTERFACE
(header-only) target.

## Benchmarks

Self-contained micro-benchmarks live in `benchmarks/`:

```bash
cmake --preset bench
cmake --build --preset bench
./build/bench/benchmarks/bench_logger   # sync / filtered / async / async-mt
./build/bench/benchmarks/bench_queue    # MPMC queue micro-benchmark
```

An optional spdlog comparison (`sudo apt install libspdlog-dev`):

```bash
cmake --preset bench -DZEST_BUILD_SPDLOG_BENCH=ON
cmake --build --preset bench
./build/bench/benchmarks/bench_spdlog
```

Representative numbers (500k iterations, Release, null sink, gcc 15):

| scenario | zest | spdlog |
|---|---|---|
| sync → null | 69 ns/op | 54 ns/op |
| async (1 producer) → null | 101 ns/op | 324 ns/op |
| async (4 producers) → null | 183 ns/op | 544 ns/op |

## Development

- **Format** — `cmake --build . --target format` (apply) /
  `check-format` (verify), using `.clang-format`.
- **Static analysis** — `-DZEST_ENABLE_CLANG_TIDY=ON` (see `.clang-tidy`).
- **Coverage** — `cmake --preset coverage && cmake --build --preset coverage &&
  ctest --preset coverage`, then capture with `lcov`.
- **API docs** — `cmake --build . --target docs` (requires Doxygen).

The `CI` workflow (manual dispatch) runs the 3-platform build/test matrix,
sanitizers, fmt fallback, and `check-format`/`clang-tidy`/`coverage` gates.

## Project layout

```
include/zest/    public headers (header-only library)
tests/            unit tests (doctest, run through CTest)
examples/         small usage examples
benchmarks/       micro-benchmarks (std::chrono)
docs/             architecture and design notes
cmake/            CMake helpers and package-config template
.github/workflows CI (GitHub Actions, preset-driven)
```

## Documentation

- [`docs/architecture.md`](docs/architecture.md) — layers and threading model.
- [`docs/design-decisions.md`](docs/design-decisions.md) — rationale and non-goals.

## Versioning

This project follows [Semantic Versioning](https://semver.org/). Changes are
tracked in [CHANGELOG.md](CHANGELOG.md); releases are tagged `vX.Y.Z`.

## License

[MIT](LICENSE)
