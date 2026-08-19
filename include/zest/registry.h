#pragma once

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>
#include <unordered_map>

#include <zest/common.h>
#include <zest/logger.h>
#include <zest/sink.h>
#include <zest/sinks/console_sink.h>

namespace zest {

// Global registry of named loggers. A Meyers singleton provides thread-safe,
// lazy initialization; the operations themselves are mutex-guarded.
class registry {
public:
  static registry& instance() {
    static registry r;
    return r;
  }

  registry(const registry&) = delete;
  registry& operator=(const registry&) = delete;

  void register_logger(std::shared_ptr<logger> l) {
    std::lock_guard<std::mutex> lock(mutex_);
    loggers_[l->name()] = l;
    if (!default_logger_) {
      default_logger_ = l;
    }
  }

  std::shared_ptr<logger> get(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = loggers_.find(name);
    return (it != loggers_.end()) ? it->second : nullptr;
  }

  void set_default_logger(std::shared_ptr<logger> l) {
    std::lock_guard<std::mutex> lock(mutex_);
    default_logger_ = std::move(l);
    if (default_logger_) {
      loggers_[default_logger_->name()] = default_logger_;
    }
  }

  std::shared_ptr<logger> default_logger() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!default_logger_) {
      default_logger_ =
          std::make_shared<logger>("default", std::make_shared<sinks::console_sink>());
      loggers_["default"] = default_logger_;
    }
    return default_logger_;
  }

  void drop(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    loggers_.erase(name);
    if (default_logger_ && default_logger_->name() == name) {
      default_logger_.reset();
    }
  }

  void drop_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    loggers_.clear();
    default_logger_.reset();
  }

  void flush_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [name, l] : loggers_) {
      (void)name;
      l->flush();
    }
  }

  // Start (or restart) a background thread that flushes every registered logger
  // every `interval`. A non-positive interval stops the periodic flusher. The
  // flusher also stops on shutdown() and when the registry is destroyed.
  void flush_every(std::chrono::milliseconds interval) {
    std::lock_guard<std::mutex> lock(flush_mutex_);
    stop_periodic_flush_locked();
    if (interval.count() <= 0) {
      return;
    }
    periodic_flusher_ = std::jthread([this, interval](std::stop_token st) {
      while (true) {
        {
          std::unique_lock<std::mutex> lk(flush_sleep_mutex_);
          // wait_for with a stop_token wakes on stop OR on timeout; it returns
          // the predicate's value, so `true` means stop was requested.
          if (flush_cv_.wait_for(lk, st, interval, [&] { return st.stop_requested(); })) {
            break;
          }
        }
        flush_all();
      }
    });
  }

  void shutdown() {
    stop_periodic_flush(); // stop the flusher before draining
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [name, l] : loggers_) {
      (void)name;
      l->flush();
    }
    loggers_.clear();
    default_logger_.reset();
  }

private:
  registry() = default;

  ~registry() { stop_periodic_flush(); } // join the flusher before members die

  void stop_periodic_flush() {
    std::lock_guard<std::mutex> lock(flush_mutex_);
    stop_periodic_flush_locked();
  }

  void stop_periodic_flush_locked() {
    if (periodic_flusher_.joinable()) {
      periodic_flusher_.request_stop();
      flush_cv_.notify_all();
      periodic_flusher_.join();
    }
  }

  std::mutex mutex_;
  std::unordered_map<std::string, std::shared_ptr<logger>> loggers_;
  std::shared_ptr<logger> default_logger_;

  // Periodic-flush state. `periodic_flusher_` is declared LAST so that on
  // destruction it is joined FIRST (reverse order), while the mutexes and the
  // condition variable it uses are still alive.
  std::mutex flush_mutex_;
  std::mutex flush_sleep_mutex_;
  std::condition_variable_any flush_cv_;
  std::jthread periodic_flusher_;
};

// -- Free functions ---------------------------------------------------------

ZEST_INLINE std::shared_ptr<logger> get_logger(const std::string& name) {
  return registry::instance().get(name);
}

ZEST_INLINE std::shared_ptr<logger> default_logger() {
  return registry::instance().default_logger();
}

ZEST_INLINE void set_default_logger(std::shared_ptr<logger> l) {
  registry::instance().set_default_logger(std::move(l));
}

ZEST_INLINE void drop_logger(const std::string& name) {
  registry::instance().drop(name);
}

ZEST_INLINE void drop_all() {
  registry::instance().drop_all();
}

ZEST_INLINE void flush_all() {
  registry::instance().flush_all();
}

ZEST_INLINE void shutdown() {
  registry::instance().shutdown();
}

// Flush all registered loggers every `interval` on a background thread.
// Pass a non-positive interval (or call shutdown()) to stop the flusher.
template <typename Rep, typename Period>
ZEST_INLINE void flush_every(std::chrono::duration<Rep, Period> interval) {
  registry::instance().flush_every(std::chrono::duration_cast<std::chrono::milliseconds>(interval));
}

} // namespace zest
