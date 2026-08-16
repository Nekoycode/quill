# quill

A lightweight, highly-customizable and thread-safe C++20 logging library.

`quill` combines an **spdlog-like architecture** (logger → sinks → pattern
formatter → level → registry) with a **frontend/backend async design** for
reliable concurrent use: the hot path only captures arguments, while a
background thread formats and performs the I/O through a bounded lock-free
queue (deferred formatting, unlike spdlog's frontend-formatting async model).

## Highlights

- **Header-only, zero mandatory dependencies** — built on the C++20 standard
  library (`std::format`, `std::chrono`, `std::source_location`,
  `std::jthread`, `std::stop_token`).
- **spdlog-like API** — `logger`, `sinks`, pattern formatter, level filtering
  and a global registry feel familiar to spdlog users.
- **Reliable concurrency** — an async logger with a bounded lock-free MPMC
  queue and a background writer thread; graceful shutdown drains all pending
  messages. The frontend only captures arguments (deferred formatting).
- **Backtrace** — keep the last N records in a ring buffer and replay them after
  an error (`enable_backtrace` / `dump_backtrace`).
- **Structured logging** — a `json_sink` that emits one JSON object per record.
- **Compile-time format string checking** and **compile-time level gating**
  (`QUILL_ACTIVE_LEVEL`).
- **Highly customizable** — custom sinks, custom formatters, custom pattern
  flags, per-sink levels/patterns.
- **Modern CMake** — target-based build, CMake presets for local builds and CI,
  install/export with `find_package(quill CONFIG)` support.

## Requirements

- C++20 compiler (GCC ≥ 13, Clang ≥ 15, MSVC ≥ 19.29).
- CMake ≥ 3.25.
- [Ninja](https://ninja-build.org/) (recommended generator; presets default to
  it).

## Quick start

```cpp
#include <quill/quill.h>

int main() {
  auto logger = quill::stdout_logger("app");
  logger->set_pattern("%^[%H:%M:%S.%e] [%l] %v%$");

  QUILL_INFO(logger, "hello {}!", "world");
  return 0;
}
```

### Building with CMake presets

```bash
cmake --preset dev          # configure (Debug + tests + examples + -Werror)
cmake --build --preset dev  # build
ctest  --preset dev         # run tests
```

Available configure presets: `dev`, `debug`, `release`, `relwithdebinfo`,
`bench`, `ci`, `asan`, `tsan`, `coverage`.

### Consuming from another CMake project

```cmake
find_package(quill CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE quill::quill)
```

## Development

- **Format** — `cmake --build . --target format` (apply) and
  `cmake --build . --target check-format` (verify), using `.clang-format`.
- **Static analysis** — configure with `-DQUILL_ENABLE_CLANG_TIDY=ON` to run
  `clang-tidy` (see `.clang-tidy`) during compilation.
- **Coverage** — `cmake --preset coverage && cmake --build --preset coverage &&
  ctest --preset coverage`, then capture with `lcov`/`gcovr` (see CI job).
- **API docs** — `cmake --build . --target docs` (requires Doxygen; see
  `Doxyfile`).
- **Design notes** — see [`docs/architecture.md`](docs/architecture.md) and
  [`docs/design-decisions.md`](docs/design-decisions.md).

The `CI` workflow (manual dispatch) runs the cross-platform build/test matrix,
sanitizers, fmt fallback, plus `check-format`, `clang-tidy` and `coverage` as
quality gates.

## Benchmarks

Self-contained micro-benchmarks (`std::chrono`, no external dependency) live in
`benchmarks/`. Build and run them with the Release-based preset:

```bash
cmake --preset bench
cmake --build --preset bench
./build/bench/benchmarks/bench_logger   # sync / filtered / async / async-mt
./build/bench/benchmarks/bench_queue    # MPMC queue micro-benchmark
```

Each binary accepts an optional iteration count, e.g.
`./build/bench/benchmarks/bench_logger 5000000`.

An optional spdlog comparison benchmark is available when spdlog is installed
(`sudo apt install libspdlog-dev`):

```bash
cmake --preset bench -DQUILL_BUILD_SPDLOG_BENCH=ON
cmake --build --preset bench
./build/bench/benchmarks/bench_spdlog
```

Representative numbers (500k iterations, Release, null sink, gcc 15):

| scenario | quill | spdlog |
|---|---|---|
| sync → null | 72 ns/op | 53 ns/op |
| async (1 producer) → null | 94 ns/op | 345 ns/op |
| async (4 producers) → null | 178 ns/op | 528 ns/op |

The async path — quill's differentiator (deferred formatting + allocation-free
frontend + backend thread pool) — is ~3× faster than spdlog's frontend-formatting
async logger; the synchronous path is close but slightly behind.

> Note: with a null sink, the async logger shows a higher per-op cost than the
> synchronous one because it pays for queue + thread handoff with no I/O to
> hide it behind. Async wins once the sink performs real I/O (the backend
> writes off the hot path).

## Layout

```
include/quill/    public headers (header-only library)
tests/            unit tests (doctest, run through CTest)
examples/         small usage examples
benchmarks/       micro-benchmarks (std::chrono)
docs/             architecture and design notes
cmake/            CMake helpers and package-config template
.github/workflows CI (GitHub Actions, preset-driven)
```

## License

[MIT](LICENSE)
