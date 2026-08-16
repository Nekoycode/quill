#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <quill/common.h>
#include <quill/detail/format.h>
#include <quill/detail/os.h>
#include <quill/level.h>
#include <quill/log_msg.h>
#include <quill/sink.h>
#include <quill/source_loc.h>

namespace quill {

// A named logger with one or more sinks.
//
// `log` takes the source location as its first parameter and is the method the
// logging macros call (capturing the call site). The `trace`/`debug`/...,
// `log_runtime` and `log_raw` convenience methods are intended for direct,
// non-macro use and record an empty source location, mirroring spdlog.
//
// `logger` is the synchronous implementation: each call formats and writes
// inline. `async_logger` overrides the write path to hand work to a background
// thread.
class logger {
public:
  explicit logger(std::string name, std::vector<std::shared_ptr<sinks::sink>> sinks)
      : name_(std::move(name)), sinks_(std::move(sinks)) {}

  explicit logger(std::string name, std::shared_ptr<sinks::sink> s)
      : name_(std::move(name)) {
    sinks_.push_back(std::move(s));
  }

  virtual ~logger() = default;

  logger(const logger&) = delete;
  logger& operator=(const logger&) = delete;

  // -- Core logging --------------------------------------------------------

  template <typename... Args>
  void log(source_loc loc, quill::level lvl, detail::format_string_t<Args...> fmt,
           Args&&... args) {
    log_formatted(lvl, detail::format(fmt, std::forward<Args>(args)...), loc);
  }

  template <typename... Args>
  void log_runtime(quill::level lvl, std::string_view fmt, Args&&... args) {
    log_formatted(lvl, detail::vformat(fmt, detail::make_format_args(args...)),
                  source_loc{});
  }

  void log_raw(quill::level lvl, std::string_view message) {
    log_formatted(lvl, std::string(message), source_loc{});
  }

  template <typename... Args>
  void trace(detail::format_string_t<Args...> fmt, Args&&... args) {
    log(source_loc{}, quill::level::trace, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void debug(detail::format_string_t<Args...> fmt, Args&&... args) {
    log(source_loc{}, quill::level::debug, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void info(detail::format_string_t<Args...> fmt, Args&&... args) {
    log(source_loc{}, quill::level::info, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void warn(detail::format_string_t<Args...> fmt, Args&&... args) {
    log(source_loc{}, quill::level::warn, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void error(detail::format_string_t<Args...> fmt, Args&&... args) {
    log(source_loc{}, quill::level::error, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void critical(detail::format_string_t<Args...> fmt, Args&&... args) {
    log(source_loc{}, quill::level::critical, fmt, std::forward<Args>(args)...);
  }

  // -- Configuration -------------------------------------------------------

  bool should_log(quill::level lvl) const noexcept {
    return lvl >= level_.load(std::memory_order_relaxed);
  }

  void set_level(quill::level lvl) noexcept {
    level_.store(lvl, std::memory_order_relaxed);
  }
  quill::level level() const noexcept { return level_.load(std::memory_order_relaxed); }

  void flush_on(quill::level lvl) noexcept {
    flush_level_.store(lvl, std::memory_order_relaxed);
  }
  quill::level flush_level() const noexcept {
    return flush_level_.load(std::memory_order_relaxed);
  }

  void set_pattern(std::string pattern) {
    for (auto& s : sinks_) {
      s->set_pattern(pattern);
    }
  }

  virtual void flush() {
    for (auto& s : sinks_) {
      s->flush();
    }
  }

  const std::string& name() const noexcept { return name_; }

  const std::vector<std::shared_ptr<sinks::sink>>& sinks() const noexcept {
    return sinks_;
  }
  std::vector<std::shared_ptr<sinks::sink>>& sinks() noexcept { return sinks_; }

protected:
  // Write path. The default implementation writes synchronously; async_logger
  // overrides it to enqueue the line.
  virtual void sink_it_(std::size_t sink_index, std::string&& line) {
    if (sink_index < sinks_.size()) {
      sinks_[sink_index]->write(line);
    }
  }

  std::string name_;
  std::vector<std::shared_ptr<sinks::sink>> sinks_;
  std::atomic<quill::level> level_{quill::level::trace};
  std::atomic<quill::level> flush_level_{quill::level::off};

private:
  void log_formatted(quill::level lvl, std::string message_text, source_loc loc) {
    if (!should_log(lvl)) {
      return;
    }

    log_msg meta;
    meta.lvl = lvl;
    meta.time = std::chrono::system_clock::now();
    meta.logger_name = name_;
    meta.thread_id = detail::get_thread_id();
    meta.loc = loc;
    meta.payload = std::move(message_text);

    for (std::size_t i = 0; i < sinks_.size(); ++i) {
      auto& s = sinks_[i];
      if (!s->should_log(lvl)) {
        continue;
      }
      std::string line;
      line.reserve(meta.payload.size() + 64);
      s->formatter().format(meta, line);
      line.push_back('\n');
      sink_it_(i, std::move(line));
    }

    if (lvl >= flush_level_.load(std::memory_order_relaxed)) {
      flush();
    }
  }
};

} // namespace quill
