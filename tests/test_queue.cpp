#include <doctest/doctest.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

#include <zest/detail/blocking_queue.h>

#include "test_config.h"

TEST_CASE("mpmc queue preserves SPSC order") {
  zest::detail::mpmc_bounded_queue<std::uint64_t> q(1024);
  for (int i = 0; i < ZEST_TEST_ITERS(1000); ++i) {
    REQUIRE(q.try_enqueue(std::uint64_t(i)));
  }
  for (int i = 0; i < ZEST_TEST_ITERS(1000); ++i) {
    std::uint64_t v = 0;
    REQUIRE(q.try_dequeue(v));
    CHECK(v == static_cast<std::uint64_t>(i));
  }
  CHECK(q.empty());
}

TEST_CASE("mpmc queue reports full and empty") {
  zest::detail::mpmc_bounded_queue<std::uint64_t> q(4); // rounded up to a power of two
  CHECK(q.empty());
  for (int i = 0; i < 4; ++i) {
    CHECK(q.try_enqueue(std::uint64_t(i)));
  }
  CHECK(q.full());
  CHECK_FALSE(q.try_enqueue(std::uint64_t(99))); // full
  std::uint64_t v = 0;
  CHECK(q.try_dequeue(v));
  CHECK(v == 0);
  CHECK_FALSE(q.full());
}

TEST_CASE("mpmc queue MPMC loses nothing and duplicates nothing") {
  zest::detail::mpmc_bounded_queue<std::uint64_t> q(1024);
  constexpr int producers = 4;
  constexpr int per = ZEST_TEST_ITERS(5000);
  constexpr std::size_t total = static_cast<std::size_t>(producers) * per;

  std::atomic<bool> done{false};
  std::vector<std::uint64_t> got;
  got.reserve(total);

  std::thread consumer([&] {
    while (!done.load(std::memory_order_acquire) || !q.empty()) {
      std::uint64_t v = 0;
      if (q.try_dequeue(v)) {
        got.push_back(v);
      } else {
        std::this_thread::yield();
      }
    }
  });

  std::vector<std::thread> ts;
  ts.reserve(producers);
  for (int p = 0; p < producers; ++p) {
    ts.emplace_back([&q, p] {
      for (int i = 0; i < per; ++i) {
        while (!q.try_enqueue((static_cast<std::uint64_t>(p) * 1000000ULL) +
                              static_cast<std::uint64_t>(i))) {
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

  REQUIRE(got.size() == total);
  std::ranges::sort(got);
  CHECK(std::ranges::adjacent_find(got) == got.end());
}

TEST_CASE("blocking queue round-trips with a consumer thread") {
  zest::detail::blocking_queue<std::uint64_t> q(16);

  std::vector<std::uint64_t> got;
  std::thread consumer([&q, &got] {
    for (int i = 0; i < 100; ++i) {
      std::uint64_t v = 0;
      const std::stop_token st;
      CHECK(q.dequeue(v, st));
      got.push_back(v);
    }
  });

  for (int i = 0; i < 100; ++i) {
    q.enqueue(std::uint64_t(i));
  }

  consumer.join();

  REQUIRE(got.size() == 100);
  CHECK(got.front() == 0);
  CHECK(got.back() == 99);
}
