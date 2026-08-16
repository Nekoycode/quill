#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>

#include <quill/level.h>
#include <quill/source_loc.h>

namespace quill {

// Metadata for a single log record plus the message text (`payload`, used as
// the `%v` value by pattern formatters).
//
// `logger_name` is a view into the owning logger's name: records never outlive
// their logger, so this avoids a per-record copy. `payload` is empty on the
// deferred async path and filled by the backend before writing.
struct log_msg {
  level lvl{level::info};
  std::chrono::system_clock::time_point time{};
  std::string_view logger_name;
  std::uint64_t thread_id{0};
  source_loc loc{};
  std::string payload;
};

} // namespace quill
