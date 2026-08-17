#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

#include <zest/formatter.h>
#include <zest/level.h>
#include <zest/log_msg.h>
#include <zest/pattern_formatter.h>

namespace zest::sinks {

// Base class for all sinks. A sink owns a formatter and a level and is
// responsible for turning a structured `log_msg` into output.
//
// The default `write` applies the pattern formatter and forwards the resulting
// line to `write_output`. Structured sinks (e.g. json_sink) override `write`
// and serialize the record themselves. Implementations must be thread-safe:
// `write`/`flush` may be called from multiple threads.
class sink {
public:
  sink() : formatter_(std::make_unique<pattern_formatter>()) {}
  explicit sink(std::unique_ptr<zest::formatter> f) : formatter_(std::move(f)) {}

  virtual ~sink() = default;

  sink(const sink&) = delete;
  sink& operator=(const sink&) = delete;

  // Entry point. `msg.payload` holds the pre-formatted message text (`%v`).
  virtual void write(const log_msg& msg) {
    zest::format_buffer line;
    {
      // Hold the mutex while reading/using formatter_, which set_formatter/
      // set_pattern/set_color mutate under the same lock.
      std::lock_guard<std::mutex> lock(mutex_);
      formatter_->format(msg, line);
    }
    line.push_back('\n');
    write_output(line.view());
  }

  virtual void flush() = 0;

  void set_level(zest::level lvl) noexcept { level_.store(lvl, std::memory_order_relaxed); }
  zest::level level() const noexcept { return level_.load(std::memory_order_relaxed); }
  bool should_log(zest::level lvl) const noexcept {
    return lvl >= level_.load(std::memory_order_relaxed);
  }

  void set_formatter(std::unique_ptr<zest::formatter> f) {
    if (!f) {
      return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    formatter_ = std::move(f);
    formatter_->set_color(color_enabled_);
  }

  void set_pattern(std::string pattern) {
    set_formatter(std::make_unique<pattern_formatter>(std::move(pattern)));
  }

  // Configures color emission. Sinks that can produce colored output call this
  // (e.g. based on a TTY check); the setting is forwarded to the formatter.
  void set_color(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    color_enabled_ = enabled;
    if (formatter_) {
      formatter_->set_color(enabled);
    }
  }

  zest::formatter& formatter() noexcept { return *formatter_; }
  const zest::formatter& formatter() const noexcept { return *formatter_; }

protected:
  // Raw I/O: writes an already-formatted line to the sink's destination.
  virtual void write_output(std::string_view line) = 0;

  mutable std::mutex mutex_;
  std::unique_ptr<zest::formatter> formatter_;
  std::atomic<zest::level> level_{zest::level::trace};
  bool color_enabled_{false};
};

} // namespace zest::sinks
