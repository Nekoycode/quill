#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <quill/common.h>
#include <quill/detail/circular_buffer.h>
#include <quill/detail/deferred_message.h>
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
// inline. `async_logger` defers the formatting to a background thread.
class logger {
public:
  explicit logger(std::string name, std::vector<std::shared_ptr<sinks::sink>> sinks)
      : name_(std::move(name)), sinks_(std::move(sinks)) {}

  explicit logger(std::string name, std::shared_ptr<sinks::sink> s) : name_(std::move(name)) {
    sinks_.push_back(std::move(s));
  }

  virtual ~logger() = default;

  logger(const logger&) = delete;
  logger& operator=(const logger&) = delete;

  // -- Core logging --------------------------------------------------------

  template <typename... Args>
  void log(source_loc loc, quill::level lvl, detail::format_string_t<Args...> fmt, Args&&... args) {
    if (!should_log(lvl)) {
      return;
    }

    log_msg meta = make_meta(lvl, loc);

    if (defers_formatting_) {
      // Deferred: capture the arguments without formatting; the backend formats.
      const auto sv = fmt.get();
      auto deferred = detail::deferred_message::make(std::string_view(sv.data(), sv.size()),
                                                     std::forward<Args>(args)...);

      if (backtrace_enabled_.load(std::memory_order_relaxed)) {
        // Backtrace is a diagnostic feature: format now just for the capture.
        std::string text;
        deferred.format_into(text);
        log_msg bm = meta;
        bm.payload = std::move(text);
        push_backtrace(bm);
      }
      log_meta(std::move(meta), std::move(deferred));
    } else {
      meta.payload = detail::format(fmt, std::forward<Args>(args)...);
      push_backtrace(meta);
      log_meta(std::move(meta), detail::deferred_message{});
    }

    flush_if_needed(lvl);
  }

  template <typename... Args>
  void log_runtime(quill::level lvl, std::string_view fmt, Args&&... args) {
    if (!should_log(lvl)) {
      return;
    }
    log_msg meta = make_meta(lvl, source_loc{});
    meta.payload = detail::vformat(fmt, detail::make_format_args(args...));
    push_backtrace(meta);
    log_meta(std::move(meta), detail::deferred_message{});
    flush_if_needed(lvl);
  }

  void log_raw(quill::level lvl, std::string_view message) {
    if (!should_log(lvl)) {
      return;
    }
    log_msg meta = make_meta(lvl, source_loc{});
    meta.payload = std::string(message);
    push_backtrace(meta);
    log_meta(std::move(meta), detail::deferred_message{});
    flush_if_needed(lvl);
  }

  template <typename... Args> void trace(detail::format_string_t<Args...> fmt, Args&&... args) {
    log(source_loc{}, quill::level::trace, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args> void debug(detail::format_string_t<Args...> fmt, Args&&... args) {
    log(source_loc{}, quill::level::debug, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args> void info(detail::format_string_t<Args...> fmt, Args&&... args) {
    log(source_loc{}, quill::level::info, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args> void warn(detail::format_string_t<Args...> fmt, Args&&... args) {
    log(source_loc{}, quill::level::warn, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args> void error(detail::format_string_t<Args...> fmt, Args&&... args) {
    log(source_loc{}, quill::level::error, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args> void critical(detail::format_string_t<Args...> fmt, Args&&... args) {
    log(source_loc{}, quill::level::critical, fmt, std::forward<Args>(args)...);
  }

  // -- Configuration -------------------------------------------------------

  bool should_log(quill::level lvl) const noexcept {
    return lvl >= level_.load(std::memory_order_relaxed);
  }

  void set_level(quill::level lvl) noexcept { level_.store(lvl, std::memory_order_relaxed); }
  quill::level level() const noexcept { return level_.load(std::memory_order_relaxed); }

  void flush_on(quill::level lvl) noexcept { flush_level_.store(lvl, std::memory_order_relaxed); }
  quill::level flush_level() const noexcept { return flush_level_.load(std::memory_order_relaxed); }

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

  // -- Backtrace -----------------------------------------------------------
  //
  // When enabled, the most recent `n` log records are kept in a ring buffer.
  // `dump_backtrace` re-emits them through the sinks (useful after an error or
  // a crash handler).

  void enable_backtrace(std::size_t n) {
    if (n == 0) {
      disable_backtrace();
      return;
    }
    std::lock_guard<std::mutex> lock(backtrace_mutex_);
    backtrace_ = std::make_unique<detail::circular_buffer<log_msg>>(n);
    backtrace_enabled_.store(true, std::memory_order_relaxed);
  }

  void disable_backtrace() {
    // Stop capturing but keep the buffer so a later dump_backtrace() can still
    // replay the most recent records.
    backtrace_enabled_.store(false, std::memory_order_relaxed);
  }

  void dump_backtrace() {
    std::vector<log_msg> msgs;
    {
      std::lock_guard<std::mutex> lock(backtrace_mutex_);
      if (backtrace_) {
        msgs = backtrace_->snapshot();
      }
    }
    for (auto& m : msgs) {
      for (auto& s : sinks_) {
        s->write(m);
      }
    }
  }

  const std::string& name() const noexcept { return name_; }

  const std::vector<std::shared_ptr<sinks::sink>>& sinks() const noexcept { return sinks_; }
  std::vector<std::shared_ptr<sinks::sink>>& sinks() noexcept { return sinks_; }

protected:
  logger(std::string name, std::vector<std::shared_ptr<sinks::sink>> sinks, bool defers_formatting)
      : name_(std::move(name)), sinks_(std::move(sinks)), defers_formatting_(defers_formatting) {}

  // Dispatch point. The default formats `deferred` (if any) and writes
  // synchronously; async_logger overrides it to enqueue the record.
  virtual void log_meta(log_msg&& meta, detail::deferred_message&& deferred) {
    if (!deferred.empty()) {
      std::string text;
      deferred.format_into(text);
      meta.payload = std::move(text);
    }
    write_record(meta);
  }

  std::string name_;
  std::vector<std::shared_ptr<sinks::sink>> sinks_;
  std::atomic<quill::level> level_{quill::level::trace};
  std::atomic<quill::level> flush_level_{quill::level::off};

private:
  log_msg make_meta(quill::level lvl, source_loc loc) const {
    log_msg meta;
    meta.lvl = lvl;
    meta.time = std::chrono::system_clock::now();
    meta.logger_name = name_;
    meta.thread_id = detail::get_thread_id();
    meta.loc = loc;
    return meta;
  }

  void write_record(const log_msg& meta) {
    for (auto& s : sinks_) {
      if (s->should_log(meta.lvl)) {
        s->write(meta);
      }
    }
  }

  void push_backtrace(const log_msg& meta) {
    if (!backtrace_enabled_.load(std::memory_order_relaxed)) {
      return;
    }
    std::lock_guard<std::mutex> lock(backtrace_mutex_);
    if (backtrace_) {
      backtrace_->push(meta);
    }
  }

  void flush_if_needed(quill::level lvl) {
    if (lvl >= flush_level_.load(std::memory_order_relaxed)) {
      flush();
    }
  }

  const bool defers_formatting_{false};
  std::atomic<bool> backtrace_enabled_{false};
  std::mutex backtrace_mutex_;
  std::unique_ptr<detail::circular_buffer<log_msg>> backtrace_;
};

} // namespace quill
