#pragma once

#include <cstdio>
#include <string>

#include <zest/detail/os.h>
#include <zest/sink.h>

namespace zest::sinks {

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
    // Read->compute->apply must be atomic, otherwise two concurrent calls can
    // apply a color that corresponds to a stale mode. NOTE: we must NOT call the
    // base set_color() here — it locks mutex_ (non-recursive) and would
    // deadlock, so apply_color_locked() sets the base members directly.
    std::lock_guard<std::mutex> lock(mutex_);
    color_mode_ = mode;
    apply_color_locked();
  }

protected:
  void write_output(std::string_view line) override {
    std::lock_guard<std::mutex> lock(mutex_);
    std::fwrite(line.data(), 1, line.size(), file());
  }

private:
  std::FILE* file() const noexcept { return stream_ == stream::stdout_stream ? stdout : stderr; }

  void refresh_color() {
    std::lock_guard<std::mutex> lock(mutex_);
    apply_color_locked();
  }

  // Requires mutex_ to be held. Computes the effective color-enabled flag from
  // color_mode_ and applies it to the base class's color state directly (the
  // base set_color() also takes mutex_, so it cannot be called here).
  void apply_color_locked() {
    bool enabled = false;
    switch (color_mode_) {
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
    color_enabled_ = enabled;
    if (formatter_) {
      formatter_->set_color(enabled);
    }
  }

  stream stream_;
  color_mode color_mode_{color_mode::automatic};
};

} // namespace zest::sinks
