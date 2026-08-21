#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <zest/async_logger.h>

#include "capture_sink.h"
#include "test_config.h"

TEST_CASE("async logger delivers all messages from multiple threads") {
  auto cs = std::make_shared<zest::test::capture_sink>();
  std::vector<std::shared_ptr<zest::sinks::sink>> sinks{cs};
  auto lg = std::make_shared<zest::async_logger>("async", std::move(sinks), 1024);
  lg->set_pattern("%v");

  constexpr int n_threads = 4;
  constexpr int n_msgs = ZEST_TEST_ITERS(1000);

  std::vector<std::thread> ts;
  ts.reserve(n_threads);
  for (int t = 0; t < n_threads; ++t) {
    ts.emplace_back([lg, t] {
      for (int i = 0; i < n_msgs; ++i) {
        lg->info("t{} m{}", t, i);
      }
    });
  }
  for (auto& th : ts) {
    th.join();
  }

  lg->flush();
  CHECK(cs->count() == static_cast<std::size_t>(n_threads) * n_msgs);
}

TEST_CASE("async logger flushes pending messages before shutdown") {
  auto cs = std::make_shared<zest::test::capture_sink>();
  std::vector<std::shared_ptr<zest::sinks::sink>> sinks{cs};
  {
    auto lg = std::make_shared<zest::async_logger>("async2", std::move(sinks), 64);
    lg->set_pattern("%v");
    for (int i = 0; i < ZEST_TEST_ITERS(1000); ++i) {
      lg->info("msg {}", i);
    }
  } // destructor drains the queue

  CHECK(cs->count() == ZEST_TEST_ITERS(1000));
}

TEST_CASE("async logger with multiple backends delivers all messages") {
  auto cs = std::make_shared<zest::test::capture_sink>();
  std::vector<std::shared_ptr<zest::sinks::sink>> sinks{cs};
  auto lg = std::make_shared<zest::async_logger>("async4", std::move(sinks), 1024, /*backends=*/4);
  lg->set_pattern("%v");

  constexpr int n_threads = 8;
  constexpr int n_msgs = ZEST_TEST_ITERS(2000);

  std::vector<std::thread> ts;
  ts.reserve(n_threads);
  for (int t = 0; t < n_threads; ++t) {
    ts.emplace_back([lg, t] {
      for (int i = 0; i < n_msgs; ++i) {
        lg->info("t{} m{}", t, i);
      }
    });
  }
  for (auto& th : ts) {
    th.join();
  }

  lg->flush();
  CHECK(cs->count() == static_cast<std::size_t>(n_threads) * n_msgs);
}

TEST_CASE("async logger owns string arguments until the backend formats them") {
  auto cs = std::make_shared<zest::test::capture_sink>();
  std::vector<std::shared_ptr<zest::sinks::sink>> sinks{cs};
  auto lg = std::make_shared<zest::async_logger>("async-defer", std::move(sinks), 64);
  lg->set_pattern("%v");

  {
    std::string s = "temporary string content";
    lg->info("{}", s);
  } // s is destroyed here, BEFORE the backend formats the record

  lg->flush();
  {
    const auto lines = cs->lines();
    REQUIRE(lines.size() == 1);
    CHECK(lines[0] == "temporary string content\n");
  }

  // A string_view argument is captured by value (the view, cheaply copyable);
  // log one whose referent stays alive until after the drain.
  {
    const std::string backing = "viewed content";
    const std::string_view sv = backing;
    lg->info("{}", sv);
    lg->flush();
  }
  {
    const auto lines = cs->lines();
    REQUIRE(lines.size() == 2);
    CHECK(lines[1] == "viewed content\n");
  }
}

TEST_CASE("async logger backtrace captures deferred records and dumps the last n") {
  auto cs = std::make_shared<zest::test::capture_sink>();
  std::vector<std::shared_ptr<zest::sinks::sink>> sinks{cs};
  auto lg = std::make_shared<zest::async_logger>("async-bt", std::move(sinks), 64);
  lg->set_pattern("%v");
  lg->enable_backtrace(3);

  for (int i = 0; i < 5; ++i) {
    lg->info("b{}", i);
  }
  lg->flush();          // drain the async path (each record written once)
  lg->dump_backtrace(); // re-emit the ring (last 3 records written again)

  const auto lines = cs->lines();
  REQUIRE(lines.size() == 8); // 5 async writes + 3 backtrace re-emits
  const auto written = [&lines](const std::string& needle) {
    return std::ranges::any_of(lines, [&](const std::string& l) { return l == needle; });
  };
  CHECK(written("b2\n"));
  CHECK(written("b3\n"));
  CHECK(written("b4\n"));
  // The ring holds only the LAST 3: the re-emitted tail must be b2..b4 in order.
  CHECK(lines[5] == "b2\n");
  CHECK(lines[6] == "b3\n");
  CHECK(lines[7] == "b4\n");
}

TEST_CASE("async overflow policy block delivers every record even with a tiny queue") {
  auto cs = std::make_shared<zest::test::capture_sink>();
  std::vector<std::shared_ptr<zest::sinks::sink>> sinks{cs};
  auto lg = std::make_shared<zest::async_logger>("ovf-block", std::move(sinks),
                                                 /*queue_size=*/8, /*backend_threads=*/1,
                                                 zest::overflow_policy::block);
  lg->set_pattern("%v");

  const int n = ZEST_TEST_ITERS(500);
  for (int i = 0; i < n; ++i) {
    lg->info("m{}", i);
  }
  lg->flush();
  CHECK(cs->count() == static_cast<std::size_t>(n));
}

TEST_CASE("async overflow policy drop_newest discards when the queue is full") {
  auto cs = std::make_shared<zest::test::slow_sink>(std::chrono::milliseconds(2));
  std::vector<std::shared_ptr<zest::sinks::sink>> sinks{cs};
  auto lg = std::make_shared<zest::async_logger>("ovf-newest", std::move(sinks),
                                                 /*queue_size=*/8, /*backend_threads=*/1,
                                                 zest::overflow_policy::drop_newest);
  lg->set_pattern("%v");

  const int n = ZEST_TEST_ITERS(400);
  for (int i = 0; i < n; ++i) {
    lg->info("m{}", i);
  }
  // flush() must complete (pending_ accounting is correct), and the slow backend
  // cannot keep up, so many records are dropped.
  lg->flush();
  CHECK(cs->count() < static_cast<std::size_t>(n));
  // drop_newest discards the INCOMING record, so the oldest records survive:
  // "m0" entered the empty queue first and is never evicted.
  CHECK(cs->contains("m0\n"));
}

TEST_CASE("async overflow policy drop_oldest keeps the newest records") {
  auto cs = std::make_shared<zest::test::slow_sink>(std::chrono::milliseconds(2));
  std::vector<std::shared_ptr<zest::sinks::sink>> sinks{cs};
  auto lg = std::make_shared<zest::async_logger>("ovf-oldest", std::move(sinks),
                                                 /*queue_size=*/8, /*backend_threads=*/1,
                                                 zest::overflow_policy::drop_oldest);
  lg->set_pattern("%v");

  const int n = ZEST_TEST_ITERS(400);
  for (int i = 0; i < n; ++i) {
    lg->info("m{}", i);
  }
  // flush() must complete (pending_ accounting is correct).
  lg->flush();
  // Records were dropped (the queue overflowed)...
  CHECK(cs->count() < static_cast<std::size_t>(n));
  // ...but the newest record always survives: it evicts an older one, never itself.
  CHECK(cs->contains("m" + std::to_string(n - 1)));
}

TEST_CASE("async overflow drop_oldest under concurrent producers completes flush") {
  auto cs = std::make_shared<zest::test::slow_sink>(std::chrono::milliseconds(1));
  std::vector<std::shared_ptr<zest::sinks::sink>> sinks{cs};
  auto lg = std::make_shared<zest::async_logger>("ovf-conc", std::move(sinks),
                                                 /*queue_size=*/16, /*backend_threads=*/2,
                                                 zest::overflow_policy::drop_oldest);
  lg->set_pattern("%v");

  constexpr int n_threads = 4;
  constexpr int n_msgs = ZEST_TEST_ITERS(300);
  std::vector<std::thread> ts;
  ts.reserve(n_threads);
  for (int t = 0; t < n_threads; ++t) {
    ts.emplace_back([lg, t] {
      for (int i = 0; i < n_msgs; ++i) {
        lg->info("t{} m{}", t, i);
      }
    });
  }
  for (auto& th : ts) {
    th.join();
  }
  // Must not hang (pending_ accounting stays correct under concurrent eviction).
  lg->flush();
  // Some records may be dropped, but never more than were produced.
  CHECK(cs->count() <= static_cast<std::size_t>(n_threads) * n_msgs);
}
