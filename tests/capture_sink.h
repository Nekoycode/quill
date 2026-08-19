#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <zest/zest.h>

namespace zest::test {

// A sink that records every written line, for assertions in unit tests.
class capture_sink final : public zest::sinks::sink {
public:
  void flush() override {}

  std::vector<std::string> lines() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lines_;
  }

  std::size_t count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lines_.size();
  }

protected:
  void write_output(std::string_view line) override {
    std::lock_guard<std::mutex> lock(mutex_);
    lines_.push_back(std::string(line));
  }

private:
  std::vector<std::string> lines_;
};

// A sink that records lines but sleeps in write_output() to simulate slow I/O,
// so a fast producer can overflow the bounded async queue. write_output is
// called without the formatter mutex held, so sleeping here is safe.
class slow_sink final : public zest::sinks::sink {
public:
  explicit slow_sink(std::chrono::milliseconds delay) : delay_(delay) {}

  void flush() override {}

  std::size_t count() const {
    std::lock_guard<std::mutex> lock(lines_mutex_);
    return lines_.size();
  }

  bool contains(const std::string& needle) const {
    std::lock_guard<std::mutex> lock(lines_mutex_);
    return std::ranges::any_of(
        lines_, [&](const std::string& l) { return l.find(needle) != std::string::npos; });
  }

protected:
  void write_output(std::string_view line) override {
    std::this_thread::sleep_for(delay_);
    std::lock_guard<std::mutex> lock(lines_mutex_);
    lines_.emplace_back(line);
  }

private:
  std::chrono::milliseconds delay_;
  mutable std::mutex lines_mutex_;
  std::vector<std::string> lines_;
};

// A sink that counts how many times flush() is called.
class flush_counting_sink final : public zest::sinks::sink {
public:
  void flush() override { flushes_.fetch_add(1, std::memory_order_relaxed); }
  int flushes() const { return flushes_.load(std::memory_order_relaxed); }

protected:
  void write_output([[maybe_unused]] std::string_view line) override {}

private:
  std::atomic<int> flushes_{0};
};

} // namespace zest::test
