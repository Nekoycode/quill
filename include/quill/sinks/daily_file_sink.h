#pragma once

#include <chrono>
#include <cstdio>
#include <ctime>
#include <string>
#include <utility>

#include <quill/detail/os.h>
#include <quill/sink.h>

namespace quill::sinks {

// Daily rotation: the file name is `<base>_YYYY-MM-DD`, and a new file is
// opened once the (rollover-offset) date changes.
class daily_file_sink final : public sink {
public:
  daily_file_sink(std::string base_filename, int rotation_hour = 0, int rotation_minute = 0)
      : base_filename_(std::move(base_filename)), rotation_hour_(rotation_hour),
        rotation_minute_(rotation_minute) {
    open_for(today());
  }

  ~daily_file_sink() override {
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

protected:
  void write_output(std::string_view line) override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_ == nullptr) {
      return;
    }
    const std::string date = today();
    if (date != current_date_) {
      std::fclose(file_);
      file_ = nullptr;
      open_for(date);
    }
    if (file_ != nullptr) {
      std::fwrite(line.data(), 1, line.size(), file_);
    }
  }

private:
  std::string today() const {
    // Shift "now" backwards by the rollover offset so that a rotation happens
    // at rotation_hour:rotation_minute instead of midnight.
    const auto offset = std::chrono::seconds(rotation_hour_ * 3600 + rotation_minute_ * 60);
    const auto tp = std::chrono::system_clock::now() - offset;
    const std::time_t tt = std::chrono::system_clock::to_time_t(tp);
    std::tm t{};
#ifdef _WIN32
    localtime_s(&t, &tt);
#else
    localtime_r(&tt, &t);
#endif
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
    return buf;
  }

  void open_for(const std::string& date) {
    current_date_ = date;
    const std::string name = base_filename_ + "_" + date;
    file_ = std::fopen(name.c_str(), "ab");
  }

  std::string base_filename_;
  int rotation_hour_;
  int rotation_minute_;
  std::string current_date_;
  std::FILE* file_{nullptr};
};

} // namespace quill::sinks
