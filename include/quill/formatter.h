#pragma once

#include <memory>
#include <string>

#include <quill/log_msg.h>

namespace quill {

// A formatter renders a `log_msg` (including its pre-formatted `payload`, i.e.
// the `%v` text) into a complete output line.
class formatter {
public:
  virtual ~formatter() = default;

  virtual void format(const log_msg& msg, std::string& out) const = 0;

  // Returns a heap-allocated copy. Sinks own their formatter, so a clone is
  // needed whenever a formatter is shared.
  virtual std::unique_ptr<formatter> clone() const = 0;

  // Enable/disable ANSI color emission. The default implementation is a no-op;
  // pattern_formatter overrides it. Called by sinks to reflect their color mode.
  virtual void set_color(bool /*enabled*/) {}
};

using formatter_ptr = std::unique_ptr<formatter>;

} // namespace quill
