#pragma once

#include <string>

#include <quill/sink.h>

namespace quill::sinks {

// Discards all output. Useful for benchmarking the hot path.
class null_sink final : public sink {
public:
  void flush() override {}

protected:
  void write_output(std::string_view /*line*/) override {}
};

} // namespace quill::sinks
