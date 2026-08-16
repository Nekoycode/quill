#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include <quill/formatter.h>
#include <quill/level.h>
#include <quill/pattern_formatter.h>

namespace quill::sinks {

// Base class for all sinks. A sink owns a formatter and a level, and is
// responsible for writing a fully-formatted line and flushing buffered output.
// Implementations must be thread-safe: `write`/`flush` may be called from
// multiple threads (the protected `mutex_` is provided for that purpose).
class sink {
public:
  sink() : formatter_(std::make_unique<pattern_formatter>()) {}
  explicit sink(std::unique_ptr<quill::formatter> f) : formatter_(std::move(f)) {}

  virtual ~sink() = default;

  sink(const sink&) = delete;
  sink& operator=(const sink&) = delete;

  virtual void write(const std::string& payload) = 0;
  virtual void flush() = 0;

  void set_level(quill::level lvl) noexcept { level_ = lvl; }
  quill::level level() const noexcept { return level_; }
  bool should_log(quill::level lvl) const noexcept { return lvl >= level_; }

  void set_formatter(std::unique_ptr<quill::formatter> f) {
    std::lock_guard<std::mutex> lock(mutex_);
    formatter_ = std::move(f);
    if (formatter_) {
      formatter_->set_color(color_enabled_);
    }
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

  quill::formatter& formatter() noexcept { return *formatter_; }
  const quill::formatter& formatter() const noexcept { return *formatter_; }

protected:
  mutable std::mutex mutex_;
  std::unique_ptr<quill::formatter> formatter_;
  quill::level level_{quill::level::trace};
  bool color_enabled_{false};
};

} // namespace quill::sinks
