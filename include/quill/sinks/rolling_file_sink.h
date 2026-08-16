#pragma once

#include <cstdio>
#include <stdexcept>
#include <string>
#include <utility>

#include <quill/sink.h>

namespace quill::sinks {

// Size-based rolling (a.k.a. "rolling log"): when the current file would exceed
// `max_size`, it is renamed to `<filename>.1`, shifting older backups up to
// `<filename>.<N>`. At most `max_files` rotated files are kept. If `max_files`
// is 0, the file is simply truncated on rollover.
class rolling_file_sink final : public sink {
public:
  rolling_file_sink(std::string base_filename, std::size_t max_size, std::size_t max_files)
      : base_filename_(std::move(base_filename)), max_size_(max_size), max_files_(max_files) {
    file_ = std::fopen(base_filename_.c_str(), "ab");
    if (file_ == nullptr) {
      throw std::runtime_error("quill: failed to open log file: " + base_filename_);
    }
    // Account for any pre-existing content so the first rotation is accurate.
    if (std::fseek(file_, 0, SEEK_END) == 0) {
      const long pos = std::ftell(file_);
      current_size_ = (pos >= 0) ? static_cast<std::size_t>(pos) : 0;
    }
  }

  ~rolling_file_sink() override {
    if (file_ != nullptr) {
      std::fclose(file_);
      file_ = nullptr;
    }
  }

  void flush() override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_ != nullptr) {
      std::fflush(file_);
    }
  }

  const std::string& filename() const noexcept { return base_filename_; }

protected:
  void write_output(std::string_view line) override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_ == nullptr) {
      return;
    }
    if (current_size_ + line.size() > max_size_) {
      rotate();
    }
    if (file_ != nullptr) {
      const std::size_t written = std::fwrite(line.data(), 1, line.size(), file_);
      current_size_ += written;
    }
  }

private:
  void rotate() {
    if (file_ != nullptr) {
      std::fclose(file_);
      file_ = nullptr;
    }

    if (max_files_ > 0) {
      std::remove((base_filename_ + "." + std::to_string(max_files_)).c_str());
      for (std::size_t i = max_files_ - 1; i >= 1; --i) {
        const std::string src = base_filename_ + "." + std::to_string(i);
        const std::string dst = base_filename_ + "." + std::to_string(i + 1);
        std::rename(src.c_str(), dst.c_str());
      }
      std::rename(base_filename_.c_str(), (base_filename_ + ".1").c_str());
      file_ = std::fopen(base_filename_.c_str(), "ab");
    } else {
      // No backups kept: truncate.
      file_ = std::fopen(base_filename_.c_str(), "wb");
    }

    current_size_ = 0;
  }

  std::string base_filename_;
  std::size_t max_size_;
  std::size_t max_files_;
  std::size_t current_size_{0};
  std::FILE* file_{nullptr};
};

// spdlog-compatible name for the same size-based rolling behavior.
using rotating_file_sink = rolling_file_sink;

} // namespace quill::sinks
