#include <zest/detail/blocking_queue.h>

#include <atomic>
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

  // 1) SPSC round-trip: one enqueue + one dequeue per iteration.
  {
    zest::detail::mpmc_bounded_queue<std::uint64_t> q(1024);
    const double ns = zest::bench::time_per_op(iters, [&] {
      std::uint64_t v = 1;
      while (!q.try_enqueue(std::move(v))) {
      }
      while (!q.try_dequeue(v)) {
      }
    });
    zest::bench::report("mpmc queue SPSC round-trip", ns);
  }

  // 2) MPMC throughput: four producers feeding one consumer.
  {
    zest::detail::mpmc_bounded_queue<std::uint64_t> q(65536);
    constexpr int producers = 4;
    const std::size_t per = iters / producers;

    std::atomic<bool> done{false};
    std::atomic<std::uint64_t> consumed{0};

    const auto start = std::chrono::steady_clock::now();
    std::thread consumer([&] {
      while (!done.load(std::memory_order_acquire) || !q.empty()) {
        std::uint64_t v = 0;
        if (q.try_dequeue(v)) {
          consumed.fetch_add(1, std::memory_order_relaxed);
        } else {
          std::this_thread::yield();
        }
      }
    });

    std::vector<std::thread> ts;
    for (int p = 0; p < producers; ++p) {
      ts.emplace_back([&q, per] {
        for (std::size_t i = 0; i < per; ++i) {
          while (!q.try_enqueue(static_cast<std::uint64_t>(i))) {
            std::this_thread::yield();
          }
        }
      });
    }
    for (auto& th : ts) {
      th.join();
    }
    done.store(true, std::memory_order_release);
    consumer.join();
    const auto end = std::chrono::steady_clock::now();

    const double total_ns = std::chrono::duration<double, std::nano>(end - start).count();
    const double ns_per_op = total_ns / static_cast<double>(per * producers);
    std::printf("consumed: %llu / %zu\n", static_cast<unsigned long long>(consumed.load()),
                per * producers);
    zest::bench::report("mpmc queue (4 producers + 1 consumer)", ns_per_op);
  }

  return 0;
}
