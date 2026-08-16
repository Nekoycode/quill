#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include <quill/level.h>
#include <quill/source_loc.h>

namespace quill {

// Metadata for a single log record plus the pre-formatted message text
// (`payload`, used as the `%v` value by pattern formatters).
struct log_msg {
  level lvl{level::info};
  std::chrono::system_clock::time_point time{};
  std::string logger_name;
  std::uint64_t thread_id{0};
  source_loc loc{};
  std::string payload;
};

} // namespace quill
