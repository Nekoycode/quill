#pragma once

#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <quill/detail/os.h>
#include <quill/level.h>
#include <quill/log_msg.h>
#include <quill/sink.h>

namespace quill::sinks {

// Writes one JSON object per log record, e.g.
//   {"time":"2025-08-16T10:30:00.123","level":"info","logger":"app",...}
//
// Unlike the pattern-based sinks, json_sink serializes the structured record
// directly and ignores the pattern formatter.
class json_sink final : public sink {
public:
  explicit json_sink(std::string filename, bool truncate = true)
      : filename_(std::move(filename)) {
    file_ = std::fopen(filename_.c_str(), truncate ? "wb" : "ab");
    if (file_ == nullptr) {
      throw std::runtime_error("quill: failed to open log file: " + filename_);
    }
  }

  ~json_sink() override {
    if (file_ != nullptr) {
      std::fclose(file_);
      file_ = nullptr;
    }
  }

  void write(const log_msg& msg) override {
    std::string line = to_json(msg);
    line.push_back('\n');
    write_output(line);
  }

  void flush() override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_ != nullptr) {
      std::fflush(file_);
    }
  }

  const std::string& filename() const noexcept { return filename_; }

protected:
  void write_output(const std::string& line) override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_ != nullptr) {
      std::fwrite(line.data(), 1, line.size(), file_);
    }
  }

private:
  static std::string to_json(const log_msg& msg) {
    std::string out;
    out.reserve(msg.payload.size() + 128);
    out += "{\"time\":\"";
    out += format_time(msg);
    out += "\",\"level\":\"";
    out += to_string_view(msg.lvl);
    out += "\",\"logger\":\"";
    append_json_escaped(out, msg.logger_name);
    out += "\",\"thread\":";
    detail::append_uint(out, msg.thread_id);
    out += ",\"message\":\"";
    append_json_escaped(out, msg.payload);
    out += "\",\"file\":\"";
    append_json_escaped(out, msg.loc.file_name());
    out += "\",\"line\":";
    detail::append_int(out, static_cast<std::int64_t>(msg.loc.line()));
    out += ",\"function\":\"";
    append_json_escaped(out, msg.loc.function_name());
    out += "\"}";
    return out;
  }

  static std::string format_time(const log_msg& msg) {
    const detail::tm_fields tf = detail::to_local_time(msg.time);
    char buf[80];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03d",
                  tf.year, tf.month, tf.day, tf.hour, tf.minute, tf.second,
                  tf.millisecond);
    return buf;
  }

  static void append_json_escaped(std::string& out, std::string_view s) {
    for (const char c : s) {
      switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
          if (static_cast<unsigned char>(c) < 0x20) {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "\\u%04x",
                          static_cast<unsigned char>(c));
            out += buf;
          } else {
            out.push_back(c);
          }
      }
    }
  }

  std::string filename_;
  std::FILE* file_{nullptr};
};

} // namespace quill::sinks
