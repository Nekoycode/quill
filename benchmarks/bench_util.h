#pragma once

#include <chrono>
#include <cstddef>
#include <cstdio>

namespace quill::bench {

// Runs `fn` exactly `iterations` times and returns the average wall-clock time
// per operation in nanoseconds.
template <typename Fn> double time_per_op(std::size_t iterations, Fn&& fn) {
  const auto start = std::chrono::steady_clock::now();
  for (std::size_t i = 0; i < iterations; ++i) {
    fn();
  }
  const auto end = std::chrono::steady_clock::now();
  const double total_ns = std::chrono::duration<double, std::nano>(end - start).count();
  return total_ns / static_cast<double>(iterations);
}

inline void report(const char* name, double ns_per_op) {
  std::printf("%-44s %12.1f ns/op   %14.0f ops/s\n", name, ns_per_op, 1.0e9 / ns_per_op);
}

} // namespace quill::bench
