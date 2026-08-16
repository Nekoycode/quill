#pragma once

#include <cstdio>
#include <stdexcept>
#include <string>
#include <utility>

#include <quill/sink.h>

namespace quill::sinks {

// Appends to a single file, optionally truncating it on open.
class basic_file_sink final : public sink {
public:
  explicit basic_file_sink(std::string filename, bool truncate = false)
      : filename_(std::move(filename)) {
    file_ = std::fopen(filename_.c_str(), truncate ? "wb" : "ab");
    if (file_ == nullptr) {
      throw std::runtime_error("quill: failed to open log file: " + filename_);
    }
  }

  ~basic_file_sink() override {
    if (file_ != nullptr) {
      std::fclose(file_);
      file_ = nullptr;
    }
  }

  void write(const std::string& payload) override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_ != nullptr) {
      std::fwrite(payload.data(), 1, payload.size(), file_);
    }
  }

  void flush() override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_ != nullptr) {
      std::fflush(file_);
    }
  }

  const std::string& filename() const noexcept { return filename_; }

private:
  std::string filename_;
  std::FILE* file_{nullptr};
};

} // namespace quill::sinks
