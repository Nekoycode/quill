#include <doctest/doctest.h>

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <quill/quill.h>

#include "capture_sink.h"
#include "test_config.h"
#include "test_util.h"

TEST_CASE("sync logger is safe under concurrent use") {
  auto cs = std::make_shared<quill::test::capture_sink>();
  quill::logger lg("conc", cs);
  lg.set_pattern("%v");

  constexpr int n_threads = 8;
  constexpr int n_msgs = QUILL_TEST_ITERS(2000);

  std::vector<std::thread> ts;
  ts.reserve(n_threads);
  for (int t = 0; t < n_threads; ++t) {
    ts.emplace_back([&lg, t] {
      for (int i = 0; i < n_msgs; ++i) {
        lg.info("t{} m{}", t, i);
      }
    });
  }
  for (auto& th : ts) {
    th.join();
  }

  const auto lines = cs->lines();
  const std::size_t total = static_cast<std::size_t>(n_threads) * n_msgs;
  REQUIRE(lines.size() == total);

  // Every line is intact (ends with a newline, no torn writes).
  for (const auto& l : lines) {
    CHECK(l.back() == '\n');
  }

  // No message was lost or duplicated.
  std::set<std::string> unique(lines.begin(), lines.end());
  CHECK(unique.size() == total);
}

TEST_CASE("file sink is safe under concurrent use") {
  quill::test::temp_dir td;
  const std::string path = td.file("conc.log");

  constexpr int n_threads = 4;
  constexpr int n_msgs = QUILL_TEST_ITERS(1000);

  {
    auto lg = quill::file_logger("f", path, true);
    lg->set_pattern("%v");
    std::vector<std::thread> ts;
    ts.reserve(n_threads);
    for (int t = 0; t < n_threads; ++t) {
      ts.emplace_back([&lg, t] {
        for (int i = 0; i < n_msgs; ++i) {
          lg->info("t{} m{}", t, i);
        }
      });
    }
    for (auto& th : ts) {
      th.join();
    }
    lg->flush();
  }

  const std::string content = quill::test::read_file(path);
  const std::size_t lines =
      static_cast<std::size_t>(std::count(content.begin(), content.end(), '\n'));
  CHECK(lines == static_cast<std::size_t>(n_threads) * n_msgs);
}
