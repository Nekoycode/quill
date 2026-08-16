#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include <quill/common.h>
#include <quill/logger.h>
#include <quill/sink.h>
#include <quill/sinks/console_sink.h>

namespace quill {

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
      default_logger_ = std::make_shared<logger>(
          "default", std::make_shared<sinks::console_sink>());
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

  void shutdown() {
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

  std::mutex mutex_;
  std::unordered_map<std::string, std::shared_ptr<logger>> loggers_;
  std::shared_ptr<logger> default_logger_;
};

// -- Free functions ---------------------------------------------------------

QUILL_INLINE std::shared_ptr<logger> get_logger(const std::string& name) {
  return registry::instance().get(name);
}

QUILL_INLINE std::shared_ptr<logger> default_logger() {
  return registry::instance().default_logger();
}

QUILL_INLINE void set_default_logger(std::shared_ptr<logger> l) {
  registry::instance().set_default_logger(std::move(l));
}

QUILL_INLINE void drop_logger(const std::string& name) {
  registry::instance().drop(name);
}

QUILL_INLINE void drop_all() { registry::instance().drop_all(); }

QUILL_INLINE void flush_all() { registry::instance().flush_all(); }

QUILL_INLINE void shutdown() { registry::instance().shutdown(); }

} // namespace quill
