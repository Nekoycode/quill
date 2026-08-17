#pragma once

#include <source_location>

namespace zest {

// zest models source locations with the standard C++20 facility. The default
// argument `source_loc::current()` is evaluated at the call site, so no
// `__FILE__`/`__LINE__` macros are required.
using source_loc = std::source_location;

} // namespace zest
