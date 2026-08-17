#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <zest/common.h>
#include <zest/detail/os.h>
#include <zest/formatter.h>
#include <zest/log_msg.h>

namespace zest {

// Formats log records according to a spdlog-style pattern string.
//
// Supported flags (single character after '%'):
//   %Y  year (4 digits)      %m  month (01-12)        %b  month short name
//   %d  day (01-31)          %a  weekday short name   %H  hour 24h (00-23)
//   %I  hour 12h (01-12)     %M  minute (00-59)       %S  second (00-59)
//   %e  milliseconds (3)     %f  microseconds (6)     %l  level short (T/D/I/...)
//   %L  level full name      %n  logger name          %t  thread id
//   %P  process id           %s  source (basename:line) %g source file path
//   %#  source line          %!  source function      %v  message text
//   %^  start color          %$  end color            %%  literal '%'
class pattern_formatter final : public formatter {
public:
  explicit pattern_formatter(std::string pattern = "%^[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] [%t] %v%$")
      : pattern_(std::move(pattern)) {
    compile_pattern();
  }

  void format(const log_msg& msg, format_buffer& out) const override {
    const detail::tm_fields tf =
        has_time_flag_ ? detail::to_local_time(msg.time) : detail::tm_fields{};
    for (const auto& it : items_) {
      switch (it.f) {
      case flag::literal:
        out += it.text;
        break;
      case flag::year:
        detail::append_int(out, tf.year, 4);
        break;
      case flag::month:
        detail::append_int(out, tf.month, 2);
        break;
      case flag::month_short_name:
        out += detail::month_short_name(tf.month);
        break;
      case flag::day:
        detail::append_int(out, tf.day, 2);
        break;
      case flag::weekday_short:
        out += detail::weekday_short_name(tf.weekday);
        break;
      case flag::hour_24:
        detail::append_int(out, tf.hour, 2);
        break;
      case flag::hour_12: {
        int h = tf.hour % 12;
        if (h == 0) {
          h = 12;
        }
        detail::append_int(out, h, 2);
        break;
      }
      case flag::minute:
        detail::append_int(out, tf.minute, 2);
        break;
      case flag::second:
        detail::append_int(out, tf.second, 2);
        break;
      case flag::millis:
        detail::append_int(out, tf.millisecond, 3);
        break;
      case flag::micros:
        detail::append_int(out, tf.microsecond, 6);
        break;
      case flag::level_short:
        out.push_back(short_level(msg.lvl));
        break;
      case flag::level_full:
        out += to_string_view(msg.lvl);
        break;
      case flag::logger_name:
        out += msg.logger_name;
        break;
      case flag::thread_id:
        detail::append_uint(out, msg.thread_id);
        break;
      case flag::process_id:
        detail::append_int(out, detail::process_id());
        break;
      case flag::source_short:
        out += basename(msg.loc.file_name());
        out.push_back(':');
        detail::append_int(out, static_cast<std::int64_t>(msg.loc.line()));
        break;
      case flag::source_file:
        out += msg.loc.file_name();
        break;
      case flag::source_line:
        detail::append_int(out, static_cast<std::int64_t>(msg.loc.line()));
        break;
      case flag::source_func:
        out += msg.loc.function_name();
        break;
      case flag::message:
        out += msg.payload;
        break;
      case flag::color_start:
        if (color_enabled_) {
          out += detail::level_color(msg.lvl);
        }
        break;
      case flag::color_end:
        if (color_enabled_) {
          out += detail::color_reset();
        }
        break;
      case flag::none:
        break;
      }
    }
  }

  std::unique_ptr<formatter> clone() const override {
    return std::make_unique<pattern_formatter>(*this);
  }

  void set_color(bool enabled) override { color_enabled_ = enabled; }

  void set_pattern(std::string pattern) {
    pattern_ = std::move(pattern);
    compile_pattern();
  }

  const std::string& pattern() const noexcept { return pattern_; }

private:
  enum class flag {
    none,
    literal,
    year,
    month,
    month_short_name,
    day,
    weekday_short,
    hour_24,
    hour_12,
    minute,
    second,
    millis,
    micros,
    level_short,
    level_full,
    logger_name,
    thread_id,
    process_id,
    source_short,
    source_file,
    source_line,
    source_func,
    message,
    color_start,
    color_end
  };

  struct item {
    flag f{flag::none};
    std::string text;
  };

  static flag flag_for(char c) {
    switch (c) {
    case 'Y':
      return flag::year;
    case 'm':
      return flag::month;
    case 'b':
      return flag::month_short_name;
    case 'd':
      return flag::day;
    case 'a':
      return flag::weekday_short;
    case 'H':
      return flag::hour_24;
    case 'I':
      return flag::hour_12;
    case 'M':
      return flag::minute;
    case 'S':
      return flag::second;
    case 'e':
      return flag::millis;
    case 'f':
      return flag::micros;
    case 'l':
      return flag::level_short;
    case 'L':
      return flag::level_full;
    case 'n':
      return flag::logger_name;
    case 't':
      return flag::thread_id;
    case 'P':
      return flag::process_id;
    case 's':
      return flag::source_short;
    case 'g':
      return flag::source_file;
    case '#':
      return flag::source_line;
    case '!':
      return flag::source_func;
    case 'v':
      return flag::message;
    case '^':
      return flag::color_start;
    case '$':
      return flag::color_end;
    default:
      return flag::none;
    }
  }

  static std::string_view basename(std::string_view path) {
    const std::size_t pos = path.find_last_of("/\\");
    return (pos == std::string_view::npos) ? path : path.substr(pos + 1);
  }

  static bool is_time_flag(flag f) {
    switch (f) {
    case flag::year:
    case flag::month:
    case flag::month_short_name:
    case flag::day:
    case flag::weekday_short:
    case flag::hour_24:
    case flag::hour_12:
    case flag::minute:
    case flag::second:
    case flag::millis:
    case flag::micros:
      return true;
    default:
      return false;
    }
  }

  void compile_pattern() {
    items_.clear();
    has_time_flag_ = false;
    std::string literal;
    auto flush_literal = [&]() {
      if (!literal.empty()) {
        items_.push_back({flag::literal, std::move(literal)});
        literal.clear();
      }
    };

    for (std::size_t i = 0; i < pattern_.size(); ++i) {
      const char c = pattern_[i];
      if (c != '%') {
        literal.push_back(c);
        continue;
      }
      if (i + 1 >= pattern_.size()) {
        literal.push_back('%');
        break;
      }
      const char n = pattern_[++i];
      if (n == '%') {
        literal.push_back('%');
        continue;
      }
      const flag f = flag_for(n);
      if (f == flag::none) {
        // Unknown flag: preserve it literally.
        literal.push_back('%');
        literal.push_back(n);
        continue;
      }
      if (is_time_flag(f)) {
        has_time_flag_ = true;
      }
      flush_literal();
      items_.push_back({f, std::string(1, n)});
    }
    flush_literal();
  }

  std::string pattern_;
  std::vector<item> items_;
  bool color_enabled_{false};
  bool has_time_flag_{false};
};

} // namespace zest
