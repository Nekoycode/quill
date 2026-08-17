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

Compiles a spdlog-style pattern string (`%Y %m %d %H %M %S %e %l %L %n %t %P
%s %g %# %! %v %^ %$ %%`) once into a list of items, then renders each record
into a line. Unknown flags are preserved literally.

### 4. `level` + compile-time gating

`zest::level` is a `uint8_t`-backed enum. `ZEST_ACTIVE_LEVEL` gates the
logging macros at the preprocessor level, so disabled statements are removed
entirely (no formatting, no argument evaluation).

### 5. `registry`

A thread-safe, lazily-initialized (Meyers singleton) map of named loggers,
providing `get`/`create`/`drop`, a default logger, and `flush_all`/`shutdown`.

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
