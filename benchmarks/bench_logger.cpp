#include <zest/zest.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

#include "bench_util.h"

int main(int argc, char** argv) {
  const std::size_t iters =
      (argc > 1) ? static_cast<std::size_t>(std::strtoull(argv[1], nullptr, 10)) : 1'000'000;
  std::printf("iterations per benchmark: %zu\n\n", iters);

  // 1) Synchronous logger -> null sink (formatting + dispatch, no I/O).
  {
    auto lg = zest::create_logger("sync", zest::null_sink());
    lg->set_pattern("%v");
    const double ns =
        zest::bench::time_per_op(iters, [&] { lg->info("benchmark message {}", 42); });
    zest::bench::report("sync logger -> null sink", ns);
  }

  // 2) Synchronous logger with the level filtered out at runtime.
  {
    auto lg = zest::create_logger("filtered", zest::null_sink());
    lg->set_level(zest::level::off);
    const double ns =
        zest::bench::time_per_op(iters, [&] { lg->info("benchmark message {}", 42); });
    zest::bench::report("sync logger (filtered, level=off)", ns);
  }

  // 3) Async logger -> null sink, single producer.
  {
    auto lg = zest::create_async_logger("async", 8192, 1, zest::null_sink());
    lg->set_pattern("%v");
    const double ns =
        zest::bench::time_per_op(iters, [&] { lg->info("benchmark message {}", 42); });
    lg->flush();
    zest::bench::report("async logger -> null sink (1 thread)", ns);
  }

  // 4) Async logger -> null sink, four producers, one backend.
  {
    constexpr int n_threads = 4;
    const std::size_t per_thread = iters / n_threads;
    auto lg = zest::create_async_logger("async-mt", 65536, 1, zest::null_sink());
    lg->set_pattern("%v");

    const auto start = std::chrono::steady_clock::now();
    std::vector<std::thread> ts;
    for (int t = 0; t < n_threads; ++t) {
      ts.emplace_back([lg, per_thread] {
        for (std::size_t i = 0; i < per_thread; ++i) {
          lg->info("benchmark message {}", 42);
        }
      });
    }
    for (auto& th : ts) {
      th.join();
    }
    lg->flush();
    const auto end = std::chrono::steady_clock::now();

    const double total_ns = std::chrono::duration<double, std::nano>(end - start).count();
    const double ns_per_op = total_ns / static_cast<double>(per_thread * n_threads);
    zest::bench::report("async logger -> null sink (4 producers, 1 backend)", ns_per_op);
  }

  // 5) Async logger -> null sink, four producers, four backends.
  {
    constexpr int n_threads = 4;
    const std::size_t per_thread = iters / n_threads;
    auto lg = zest::create_async_logger("async-mt4", 65536, n_threads, zest::null_sink());
    lg->set_pattern("%v");

    const auto start = std::chrono::steady_clock::now();
    std::vector<std::thread> ts;
    for (int t = 0; t < n_threads; ++t) {
      ts.emplace_back([lg, per_thread] {
        for (std::size_t i = 0; i < per_thread; ++i) {
          lg->info("benchmark message {}", 42);
        }
      });
    }
    for (auto& th : ts) {
      th.join();
    }
    lg->flush();
    const auto end = std::chrono::steady_clock::now();

    const double total_ns = std::chrono::duration<double, std::nano>(end - start).count();
    const double ns_per_op = total_ns / static_cast<double>(per_thread * n_threads);
    zest::bench::report("async logger -> null sink (4 producers, 4 backends)", ns_per_op);
  }

  return 0;
}
