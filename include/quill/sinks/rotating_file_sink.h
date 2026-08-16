#pragma once

#include <cstdio>
#include <string>
#include <utility>

#include <quill/sink.h>

namespace quill::sinks {

// Size-based rotation: when the current file would exceed `max_size`, it is
// renamed to `<filename>.1`, shifting older backups up to `<filename>.<N>`.
class rotating_file_sink final : public sink {
public:
  rotating_file_sink(std::string base_filename, std::size_t max_size,
                     std::size_t max_files)
      : base_filename_(std::move(base_filename)),
        max_size_(max_size),
        max_files_(max_files) {
    file_ = std::fopen(base_filename_.c_str(), "ab");
  }

  ~rotating_file_sink() override {
    if (file_ != nullptr) {
      std::fclose(file_);
      file_ = nullptr;
    }
  }

  void write(const std::string& payload) override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_ == nullptr) {
      return;
    }
    if (current_size_ + payload.size() > max_size_) {
      rotate();
    }
    if (file_ != nullptr) {
      std::fwrite(payload.data(), 1, payload.size(), file_);
      current_size_ += payload.size();
    }
  }

  void flush() override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_ != nullptr) {
      std::fflush(file_);
    }
  }

  const std::string& filename() const noexcept { return base_filename_; }

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
    }

    file_ = std::fopen(base_filename_.c_str(), "ab");
    current_size_ = 0;
  }

  std::string base_filename_;
  std::size_t max_size_;
  std::size_t max_files_;
  std::size_t current_size_{0};
  std::FILE* file_{nullptr};
};

} // namespace quill::sinks
