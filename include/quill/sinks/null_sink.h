#pragma once

#include <string>

#include <quill/sink.h>

namespace quill::sinks {

// Discards all output. Useful for benchmarking the hot path.
class null_sink final : public sink {
public:
  void write(const std::string& /*payload*/) override {}
  void flush() override {}
};

} // namespace quill::sinks
