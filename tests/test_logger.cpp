#include <doctest/doctest.h>

#include <memory>
#include <string>

#include "capture_sink.h"

TEST_CASE("logger formats and writes to a sink") {
  auto cs = std::make_shared<zest::test::capture_sink>();
  zest::logger lg("test", cs);
  lg.set_pattern("%v");

  lg.info("value={}", 42);
  lg.flush();

  const auto lines = cs->lines();
  REQUIRE(lines.size() == 1);
  CHECK(lines[0] == "value=42\n");
}

TEST_CASE("logger respects level filtering") {
  auto cs = std::make_shared<zest::test::capture_sink>();
  zest::logger lg("test", cs);
  lg.set_pattern("%v");
  lg.set_level(zest::level::warn);

  lg.info("hidden");
  lg.warn("shown");

  const auto lines = cs->lines();
  REQUIRE(lines.size() == 1);
  CHECK(lines[0] == "shown\n");
}

TEST_CASE("logger writes to all sinks that accept the level") {
  auto a = std::make_shared<zest::test::capture_sink>();
  auto b = std::make_shared<zest::test::capture_sink>();
  b->set_level(zest::level::error);

  std::vector<std::shared_ptr<zest::sinks::sink>> sinks{a, b};
  zest::logger lg("multi", std::move(sinks));
  lg.set_pattern("%v");

  lg.info("info");
  lg.error("error");

  CHECK(a->count() == 2);
  CHECK(b->count() == 1);
}

TEST_CASE("logger supports multiple arguments and runtime format strings") {
  auto cs = std::make_shared<zest::test::capture_sink>();
  zest::logger lg("test", cs);
  lg.set_pattern("%v");

  lg.info("{} + {} = {}", 1, 2, 3);
  lg.log_runtime(zest::level::info, "runtime {} {}", "ok", 7);
  lg.flush();

  const auto lines = cs->lines();
  REQUIRE(lines.size() == 2);
  CHECK(lines[0] == "1 + 2 = 3\n");
  CHECK(lines[1] == "runtime ok 7\n");
}

TEST_CASE("default-logger free functions forward to the default logger") {
  zest::drop_all();
  auto cs = std::make_shared<zest::test::capture_sink>();
  auto dl = zest::create_logger("ff-default", cs);
  dl->set_pattern("%v");
  zest::set_default_logger(dl);

  zest::info("hello {}", 42);
  zest::warn("careful");

  const auto lines = cs->lines();
  REQUIRE(lines.size() == 2);
  CHECK(lines[0] == "hello 42\n");
  CHECK(lines[1] == "careful\n");

  zest::drop_all(); // reset so later tests get a fresh lazy default logger
}
