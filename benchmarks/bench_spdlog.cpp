#include <zest/zest.h>

#include <spdlog/async.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <thread>
#include <vector>

#include "bench_util.h"

namespace {

// Runs `n_threads` threads, each invoking `fn` `per_thread` times; returns the
// aggregate wall-clock time per operation in nanoseconds.
template <typename Fn> double bench_mt(int n_threads, std::size_t per_thread, Fn&& fn) {
  const auto start = std::chrono::steady_clock::now();
  std::vector<std::thread> ts;
  ts.reserve(n_threads);
  for (int t = 0; t < n_threads; ++t) {
    ts.emplace_back([&fn, per_thread] {
      for (std::size_t i = 0; i < per_thread; ++i) {
        fn();
      }
    });
  }
  for (auto& th : ts) {
    th.join();
  }
  const auto end = std::chrono::steady_clock::now();
  const double total_ns = std::chrono::duration<double, std::nano>(end - start).count();
  return total_ns / static_cast<double>(per_thread * n_threads);
}

} // namespace

int main(int argc, char** argv) {
  const std::size_t iters =
      (argc > 1) ? static_cast<std::size_t>(std::strtoull(argv[1], nullptr, 10)) : 1'000'000;
  std::printf("iterations per benchmark: %zu\n\n", iters);

  // ---- sync -> null sink -------------------------------------------------
  {
    auto q = zest::create_logger("zest-sync", zest::null_sink());
    q->set_pattern("%v");
    const double ns = zest::bench::time_per_op(iters, [&] { q->info("benchmark message {}", 42); });
    zest::bench::report("zest  sync  -> null", ns);
  }
  {
    auto s = std::make_shared<spdlog::sinks::null_sink_mt>();
    auto lg = std::make_shared<spdlog::logger>("spdlog-sync", s);
    lg->set_pattern("%v");
    const double ns =
        zest::bench::time_per_op(iters, [&] { lg->info("benchmark message {}", 42); });
    zest::bench::report("spdlog sync  -> null", ns);
  }

  // ---- async (1 producer) -> null sink -----------------------------------
  {
    auto q = zest::create_async_logger("zest-async", 8192, 1, zest::null_sink());
    q->set_pattern("%v");
    const double ns = zest::bench::time_per_op(iters, [&] { q->info("benchmark message {}", 42); });
    q->flush();
    zest::bench::report("zest  async(1) -> null", ns);
  }
  {
    auto tp = std::make_shared<spdlog::details::thread_pool>(8192, 1);
    auto s = std::make_shared<spdlog::sinks::null_sink_mt>();
    auto lg = std::make_shared<spdlog::async_logger>("spdlog-async", s, tp,
                                                     spdlog::async_overflow_policy::block);
    lg->set_pattern("%v");
    const double ns =
        zest::bench::time_per_op(iters, [&] { lg->info("benchmark message {}", 42); });
    lg->flush();
    zest::bench::report("spdlog async(1) -> null", ns);
  }

  // ---- async (4 producers) -> null sink ----------------------------------
  constexpr int n_threads = 4;
  const std::size_t per_thread = iters / n_threads;
  {
    auto q = zest::create_async_logger("zest-async-mt", 65536, n_threads, zest::null_sink());
    q->set_pattern("%v");
    const double ns = bench_mt(n_threads, per_thread, [&] { q->info("benchmark message {}", 42); });
    q->flush();
    zest::bench::report("zest  async(4) -> null", ns);
  }
  {
    auto tp = std::make_shared<spdlog::details::thread_pool>(65536, n_threads);
    auto s = std::make_shared<spdlog::sinks::null_sink_mt>();
    auto lg = std::make_shared<spdlog::async_logger>("spdlog-async-mt", s, tp,
                                                     spdlog::async_overflow_policy::block);
    lg->set_pattern("%v");
    const double ns =
        bench_mt(n_threads, per_thread, [&] { lg->info("benchmark message {}", 42); });
    lg->flush();
    zest::bench::report("spdlog async(4) -> null", ns);
  }

  return 0;
}
