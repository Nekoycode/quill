#pragma once

#include <cstdio>
#include <string>

#include <quill/detail/os.h>
#include <quill/sink.h>

namespace quill::sinks {

// Writes to stdout or stderr, optionally with ANSI color.
class console_sink final : public sink {
public:
  enum class stream { stdout_stream, stderr_stream };
  enum class color_mode { automatic, always, never };

  explicit console_sink(stream s = stream::stdout_stream) : stream_(s) { refresh_color(); }

  void flush() override {
    std::lock_guard<std::mutex> lock(mutex_);
    std::fflush(file());
  }

  void set_color_mode(color_mode mode) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      color_mode_ = mode;
    }
    refresh_color();
  }

protected:
  void write_output(std::string_view line) override {
    std::lock_guard<std::mutex> lock(mutex_);
    std::fwrite(line.data(), 1, line.size(), file());
  }

private:
  std::FILE* file() const noexcept { return stream_ == stream::stdout_stream ? stdout : stderr; }

  void refresh_color() {
    color_mode mode;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      mode = color_mode_;
    }
    bool enabled = false;
    switch (mode) {
    case color_mode::always:
      enabled = true;
      break;
    case color_mode::never:
      enabled = false;
      break;
    case color_mode::automatic:
#ifdef _WIN32
      detail::enable_windows_ansi();
#endif
      enabled = detail::is_tty(file());
      break;
    }
    set_color(enabled);
  }

  stream stream_;
  color_mode color_mode_{color_mode::automatic};
};

} // namespace quill::sinks
