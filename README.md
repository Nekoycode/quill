# quill

A lightweight, highly-customizable and thread-safe C++20 logging library.

`quill` combines an **spdlog-like architecture** (logger → sinks → pattern
formatter → level → registry) with a **frontend/backend async design** for
reliable concurrent use: the hot path only formats, while a background thread
performs the actual I/O through a bounded lock-free queue.

## Highlights

- **Header-only, zero mandatory dependencies** — built on the C++20 standard
  library (`std::format`, `std::chrono`, `std::source_location`,
  `std::jthread`, `std::stop_token`).
- **spdlog-like API** — `logger`, `sinks`, pattern formatter, level filtering
  and a global registry feel familiar to spdlog users.
- **Reliable concurrency** — an async logger with a bounded lock-free MPMC
  queue and a background writer thread; graceful shutdown drains all pending
  messages.
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
`ci`, `asan`, `tsan`.

### Consuming from another CMake project

```cmake
find_package(quill CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE quill::quill)
```

## Layout

```
include/quill/    public headers (header-only library)
tests/            unit tests (doctest, run through CTest)
examples/         small usage examples
cmake/            CMake helpers and package-config template
.github/workflows CI (GitHub Actions, preset-driven)
```

## License

[MIT](LICENSE)
