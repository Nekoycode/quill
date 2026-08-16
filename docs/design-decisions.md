# Design decisions

## 1. Header-only, zero mandatory dependencies

The library is an `INTERFACE` target (`quill::quill`) that ships only headers.
Formatting uses C++20 `std::format`; an opt-in `QUILL_USE_STD_FORMAT=OFF` mode
switches to the `fmt` library for older toolchains. Rationale: minimal build
integration and no runtime dependency by default.

## 2. spdlog-like API surface

`logger`/`sink`/`pattern_formatter`/`level`/`registry` and the
`QUILL_LOGGER_*` macros deliberately mirror spdlog so that prior spdlog
experience transfers. The async *engine* differs: quill defers the message
formatting to the backend thread — the hot path captures the format string and
arguments into a `detail::deferred_message` (small-buffer-optimized: the common
case is heap-allocation-free) instead of formatting — and uses a bounded
lock-free queue, unlike spdlog's frontend-formatting async model.

## 3. Structured `log_msg` reaches the sink

The sink interface receives a `log_msg` (metadata + formatted `%v`), not a
finished string. This lets `json_sink` serialize structured records directly
instead of re-parsing a formatted line, and keeps the door open for further
deferred formatting.

## 4. Compile-time format string checking

`logger::log` takes `detail::format_string_t<Args...>`, a
`std::basic_format_string` (or `fmt::format_string`), so invalid format
strings are rejected at compile time. `log_runtime`/`log_raw` are the escape
hatches for runtime strings.

## 5. Strict `-std=c++20`

Presets set `CMAKE_CXX_EXTENSIONS=OFF` and `cxx_std_20`. POSIX facilities
(thread-safe local time, process id, tty detection) are re-exposed with
`_POSIX_C_SOURCE=200809L` (Linux/BSD) or `_DARWIN_C_SOURCE` (macOS) rather than
relying on GNU extensions.

## 6. CMake-first, presets-driven

Target-based, namespace-alias (`quill::quill`), `BUILD_INTERFACE`/
`INSTALL_INTERFACE` includes, `GNUInstallDirs` + exported `quillTargets`, and a
`find_package(quill CONFIG)` package. `CMakePresets.json` covers local dev,
CI, and sanitizers; tests are `doctest` + CTest.

## 7. Lightweight, small runtime footprint

The async hot path is allocation-free: `detail::deferred_message` is
small-buffer-optimized (64-byte inline storage, heap only for large argument
packs), and `log_msg` stores the logger name as a `std::string_view` to avoid a
per-record copy. `async_msg` is 176 bytes, so a default 4096-entry queue is
~0.7 MB. The heavy async machinery is only instantiated when `async_logger` is
used; synchronous-only programs stay minimal.

## 8. Non-goals (for now)

- A `mcpp.toml` dual-build entry (mcpp is module-first and early-stage; deferred).
- A compiled (non-header-only) library variant.
