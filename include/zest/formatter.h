#pragma once

#include <memory>

#include <zest/detail/small_buffer.h>
#include <zest/log_msg.h>

namespace zest {

// Buffer formatters write into. A 256-byte inline buffer keeps the common case
// heap-allocation-free; larger lines overflow to the heap.
using format_buffer = detail::small_buffer<256>;

// A formatter renders a `log_msg` (including its pre-formatted `payload`, i.e.
// the `%v` text) into a complete output line.
class formatter {
public:
  virtual ~formatter() = default;

  virtual void format(const log_msg& msg, format_buffer& out) const = 0;

  // Returns a heap-allocated copy. Sinks own their formatter, so a clone is
  // needed whenever a formatter is shared.
  virtual std::unique_ptr<formatter> clone() const = 0;

  // Enable/disable ANSI color emission. The default implementation is a no-op;
  // pattern_formatter overrides it. Called by sinks to reflect their color mode.
  virtual void set_color(bool /*enabled*/) {}
};

using formatter_ptr = std::unique_ptr<formatter>;

} // namespace zest
