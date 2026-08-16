#pragma once

#include <mutex>
#include <string>
#include <vector>

#include <quill/quill.h>

namespace quill::test {

// A sink that records every written line, for assertions in unit tests.
class capture_sink final : public quill::sinks::sink {
public:
  void write(const std::string& payload) override {
    std::lock_guard<std::mutex> lock(mutex_);
    lines_.push_back(payload);
  }

  void flush() override {}

  std::vector<std::string> lines() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lines_;
  }

  std::size_t count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lines_.size();
  }

private:
  std::vector<std::string> lines_;
};

} // namespace quill::test
