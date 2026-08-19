# Architecture

`zest` is a header-only C++20 logging library. Its API follows the familiar
spdlog shape (logger → sinks → pattern formatter → level → registry), while its
async engine offloads I/O to a dedicated background thread fed by a bounded
lock-free queue.

## Layers

```
user code ──► logger ──► sinks (console / file / rotating / daily / json / null)
                  │            │
                  │            └─ formatter (pattern or structured)
                  │
                  └─ (async) bounded lock-free MPMC queue ──► backend thread
```

### 1. `logger`

A named holder of one or more sinks plus configuration (level, flush level,
pattern, backtrace). The logging macros (`ZEST_LOGGER_*`) resolve to
`logger::log(source_loc, level, format_string, args...)`, capturing the call
site via `std::source_location::current()`.

The synchronous path formats the message text (`%v`) on the calling thread and
hands the structured record to each sink. `async_logger` defers formatting: the
calling thread captures the format string and arguments (type-erased, owning)
into a `detail::deferred_message` and enqueues it; the backend thread formats
the `%v` text and writes. This is the key architectural difference from spdlog,
whose async logger formats on the calling thread.

### 2. `sinks::sink`

The sink owns a formatter and a level and receives a structured `log_msg`
(level, timestamp, logger name, thread id, source location, and the formatted
`%v` text). The default `sink::write` applies the pattern formatter and forwards
the line to `write_output`, the raw I/O primitive. Structured sinks (e.g.
`json_sink`) override `write` to serialize the record directly.

Every sink protects its destination with a mutex, so a single sink may be
shared by many threads and many loggers.

### 3. `pattern_formatter`

Compiles a pattern string once into a list of items, then renders each record
into a line. Two interchangeable, freely-mixable syntaxes are supported: the
spdlog-compatible `%`-flags (`%Y %m %d %H %M %S %e %l %L %n %t %P %s %g %# %! %v
%^ %$ %%`) and zest's native brace fields (`{datetime} [{level}] {logger}
{msg}`, …), which are distinct from both spdlog's `%` and quill's `%(named)`.
Unknown flags/fields are preserved literally; `%%` and `{{`/`}}` escape.

### 4. `level` + compile-time gating

`zest::level` is a `uint8_t`-backed enum. `ZEST_ACTIVE_LEVEL` gates the
logging macros at the preprocessor level, so disabled statements are removed
entirely (no formatting, no argument evaluation).

### 5. `registry`

A thread-safe, lazily-initialized (Meyers singleton) map of named loggers,
providing `get`/`create`/`drop`, a default logger, and `flush_all`/`shutdown`.
`flush_every(interval)` additionally runs a background `std::jthread` that
flushes all loggers on a stop-token-aware schedule; it stops cleanly on a
non-positive interval, on `shutdown()`, and at registry destruction.

### 6. Async engine

`async_logger` owns a `detail::blocking_queue` (a bounded lock-free MPMC ring
buffer — Dmitry Vyukov's algorithm — wrapped with condition-variable
backpressure) and a pool of `std::jthread` backend threads (configurable,
default 1). The frontend captures the message (format string + arguments)
without formatting, into a small-buffer-optimized `detail::deferred_message`
(no heap allocation for the common case); a backend thread formats the `%v`
text and applies each sink's formatter before writing. An atomic pending-record
counter lets `flush()` wait until every enqueued record is written before
flushing the sinks, which stays correct across multiple backends. Destruction
requests a stop and drains all pending records, so no in-flight message is
lost.

When the bounded queue is full, the `overflow_policy` controls producer
behavior: `block` (default) waits for space so nothing is lost; `drop_oldest`
evicts the oldest pending record; `drop_newest` discards the incoming record.
The pending counter is kept exact under every policy (count-before-enqueue,
subtract evictions), so `flush()` and destruction drain correctly regardless of
drops.

### 7. C++20 module interface

`src/zest.cppm` provides `import zest;`. It includes all public headers in the
global module fragment (so declarations stay attached to the global module —
no ODR/ABI drift vs. the header-only build), then re-exports the public API
with `using`-declarations. Macros cannot be exported by a module, so the
`ZEST_*` macros stay header-only and the module exposes the equivalent
function/method API (the `logger::info(...)` members and the `zest::info(...)`
default-logger free functions). The module is built by mcpp (conventional
`src/zest.cppm` lib root) and by CMake via the opt-in `ZEST_BUILD_MODULE`
target `zest::module`.

## Threading model

| Component | Guarantee |
|---|---|
| `sink::write`/`flush` | thread-safe (internal mutex) |
| `logger::log` | thread-safe (atomic level check + thread-safe sinks) |
| `registry` | thread-safe (mutex) |
| `pattern_formatter::format` | const, safe to call concurrently |
| formatter/pattern reconfiguration | configure before use (not synchronized with in-flight formats) |

## Time

Timestamps are captured once per record as `std::chrono::system_clock`
time-points; the pattern formatter renders them as local time on demand.
