#include <doctest/doctest.h>

#include <memory>
#include <thread>
#include <vector>

#include <quill/async_logger.h>

#include "capture_sink.h"

TEST_CASE("async logger delivers all messages from multiple threads") {
  auto cs = std::make_shared<quill::test::capture_sink>();
  std::vector<std::shared_ptr<quill::sinks::sink>> sinks{cs};
  auto lg = std::make_shared<quill::async_logger>("async", std::move(sinks), 1024);
  lg->set_pattern("%v");

  constexpr int n_threads = 4;
  constexpr int n_msgs = 1000;

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
  auto cs = std::make_shared<quill::test::capture_sink>();
  std::vector<std::shared_ptr<quill::sinks::sink>> sinks{cs};
  {
    auto lg = std::make_shared<quill::async_logger>("async2", std::move(sinks), 64);
    lg->set_pattern("%v");
    for (int i = 0; i < 1000; ++i) {
      lg->info("msg {}", i);
    }
  } // destructor drains the queue

  CHECK(cs->count() == 1000);
}
