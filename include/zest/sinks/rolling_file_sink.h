#pragma once

#include <cstdio>
#include <stdexcept>
#include <string>
#include <utility>

#include <zest/sink.h>

namespace zest::sinks {

// Size-based rolling (a.k.a. "rolling log"): when the current file would exceed
// `max_size`, it is renamed to `<filename>.1`, shifting older backups up to
// `<filename>.<N>`. At most `max_files` rotated files are kept. If `max_files`
// is 0, the file is simply truncated on rollover.
class rolling_file_sink final : public sink {
public:
  rolling_file_sink(std::string base_filename, std::size_t max_size, std::size_t max_files)
      : base_filename_(std::move(base_filename)), max_size_(max_size), max_files_(max_files) {
    // Opens for append and accounts for any pre-existing content so the first
    // rotation is accurate.
    if (!try_reopen()) {
      throw std::runtime_error("zest: failed to open log file: " + base_filename_);
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
    if (file_ == nullptr && !try_reopen()) {
      return; // a prior open/rotate failed and reopen still fails: drop the record
    }
    // Only rotate when there is content to roll over: an oversized record on a
    // fresh (empty) file must not evict real backups for nothing.
    if (current_size_ > 0 && current_size_ + line.size() > max_size_) {
      rotate();
    }
    if (file_ != nullptr) {
      const std::size_t written = std::fwrite(line.data(), 1, line.size(), file_);
      current_size_ += written;
    }
  }

private:
  // Open base_filename_ for appending and sync current_size_ with its real
  // on-disk size. Returns false when the file cannot be opened.
  bool try_reopen() {
    file_ = std::fopen(base_filename_.c_str(), "ab");
    if (file_ == nullptr) {
      return false;
    }
    if (std::fseek(file_, 0, SEEK_END) == 0) {
      const long pos = std::ftell(file_);
      current_size_ = (pos >= 0) ? static_cast<std::size_t>(pos) : 0;
    }
    return true;
  }

  void rotate() {
    if (file_ != nullptr) {
      std::fclose(file_);
      file_ = nullptr;
    }

    bool renamed = false;
    if (max_files_ > 0) {
      std::remove((base_filename_ + "." + std::to_string(max_files_)).c_str());
      for (std::size_t i = max_files_ - 1; i >= 1; --i) {
        const std::string src = base_filename_ + "." + std::to_string(i);
        const std::string dst = base_filename_ + "." + std::to_string(i + 1);
        std::rename(src.c_str(), dst.c_str()); // best-effort; a missing backup is fine
      }
      renamed = std::rename(base_filename_.c_str(), (base_filename_ + ".1").c_str()) == 0;
    }
    // If the base rename failed, the old content is still in place — truncate
    // ("wb") rather than let the base file grow unbounded. On success ("ab") the
    // base is fresh and empty. (max_files_ == 0 keeps the original truncate.)
    file_ = std::fopen(base_filename_.c_str(), renamed ? "ab" : "wb");
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

} // namespace zest::sinks
