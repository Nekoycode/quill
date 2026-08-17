#pragma once

#include <mutex>
#include <string>
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

} // namespace zest::test
