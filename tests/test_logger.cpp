#include <doctest/doctest.h>

#include <memory>
#include <string>

#include "capture_sink.h"

TEST_CASE("logger formats and writes to a sink") {
  auto cs = std::make_shared<quill::test::capture_sink>();
  quill::logger lg("test", cs);
  lg.set_pattern("%v");

  lg.info("value={}", 42);
  lg.flush();

  const auto lines = cs->lines();
  REQUIRE(lines.size() == 1);
  CHECK(lines[0] == "value=42\n");
}

TEST_CASE("logger respects level filtering") {
  auto cs = std::make_shared<quill::test::capture_sink>();
  quill::logger lg("test", cs);
  lg.set_pattern("%v");
  lg.set_level(quill::level::warn);

  lg.info("hidden");
  lg.warn("shown");

  const auto lines = cs->lines();
  REQUIRE(lines.size() == 1);
  CHECK(lines[0] == "shown\n");
}

TEST_CASE("logger writes to all sinks that accept the level") {
  auto a = std::make_shared<quill::test::capture_sink>();
  auto b = std::make_shared<quill::test::capture_sink>();
  b->set_level(quill::level::error);

  std::vector<std::shared_ptr<quill::sinks::sink>> sinks{a, b};
  quill::logger lg("multi", std::move(sinks));
  lg.set_pattern("%v");

  lg.info("info");
  lg.error("error");

  CHECK(a->count() == 2);
  CHECK(b->count() == 1);
}

TEST_CASE("logger supports multiple arguments and runtime format strings") {
  auto cs = std::make_shared<quill::test::capture_sink>();
  quill::logger lg("test", cs);
  lg.set_pattern("%v");

  lg.info("{} + {} = {}", 1, 2, 3);
  lg.log_runtime(quill::level::info, "runtime {} {}", "ok", 7);
  lg.flush();

  const auto lines = cs->lines();
  REQUIRE(lines.size() == 2);
  CHECK(lines[0] == "1 + 2 = 3\n");
  CHECK(lines[1] == "runtime ok 7\n");
}
