// Module-consumer example: uses `import zest;` instead of `#include <zest/zest.h>`.
//
// Built only when ZEST_BUILD_MODULE=ON (requires CMake >= 3.28, a Ninja
// generator, and a module-capable compiler). The ZEST_* macros are header-only
// (C++ modules cannot export macros), so this example uses the exported
// function API instead — including the default-logger free functions.

#include <cstdio> // standard headers come before the `import`

import zest;

int main() {
  zest::set_default_logger(zest::create_logger("module-demo", zest::null_sink()));

  // Default-logger free functions (the module-facing equivalent of ZEST_INFO).
  zest::info("hello from the zest module: {}", 42);
  zest::warn("careful {}", "now");

  // Level parsing utility.
  auto lvl = zest::from_string_view("warning");
  if (!lvl || *lvl != zest::level::warn) {
    std::puts("module demo FAILED");
    return 1;
  }

  zest::shutdown();
  std::puts("module demo OK");
  return 0;
}
