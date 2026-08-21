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

TEST_CASE("flush_on flushes the sinks once the record level reaches the threshold") {
  auto fs = std::make_shared<zest::test::flush_counting_sink>();
  zest::logger lg("flush-on", fs);
  lg.set_level(zest::level::trace);
  lg.flush_on(zest::level::error);

  lg.info("below threshold");
  CHECK(fs->flushes() == 0);
  lg.warn("still below threshold");
  CHECK(fs->flushes() == 0);
  lg.error("at threshold");
  CHECK(fs->flushes() == 1);
  lg.critical("above threshold");
  CHECK(fs->flushes() == 2);
}

TEST_CASE("logger never auto-flushes when flush_on was not set") {
  auto fs = std::make_shared<zest::test::flush_counting_sink>();
  zest::logger lg("no-flush-on", fs);

  lg.critical("critical must not auto-flush by default");
  CHECK(fs->flushes() == 0);

  lg.flush(); // explicit flush still works
  CHECK(fs->flushes() == 1);
}

TEST_CASE("ZEST_* macros log to the default logger with call-site source location") {
  zest::drop_all();
  auto cs = std::make_shared<zest::test::capture_sink>();
  auto dl = zest::create_logger("md", cs);
  dl->set_pattern("%v [%s:%#]");
  zest::set_default_logger(dl);

  ZEST_INFO("hello {}", 42);
  ZEST_WARN("x");

  const auto lines = cs->lines();
  REQUIRE(lines.size() == 2);
  CHECK(lines[0].find("hello 42") != std::string::npos);
  CHECK(lines[0].find("test_logger.cpp:") != std::string::npos);

  zest::drop_all(); // reset so later tests get a fresh lazy default logger
}

TEST_CASE("ZEST_LOGGER_* macros log to an explicit logger and respect its level") {
  auto cs = std::make_shared<zest::test::capture_sink>();
  auto lg = std::make_shared<zest::logger>("macro-explicit", cs);
  lg->set_pattern("%v");
  lg->set_level(zest::level::warn);

  ZEST_LOGGER_DEBUG(lg, "hidden");
  ZEST_LOGGER_WARN(lg, "shown");

  const auto lines = cs->lines();
  REQUIRE(lines.size() == 1);
  CHECK(lines[0] == "shown\n");
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

TEST_CASE("log_raw writes the message verbatim without format interpretation") {
  auto cs = std::make_shared<zest::test::capture_sink>();
  zest::logger lg("raw", cs);
  lg.set_pattern("%v");

  // Braces in a raw message are NOT treated as format placeholders.
  lg.log_raw(zest::level::info, "not {} a {} format string");
  lg.flush();

  const auto lines2 = cs->lines();
  REQUIRE(lines2.size() == 1);
  CHECK(lines2[0] == "not {} a {} format string\n");
}

TEST_CASE("logger getters report level, flush level, name and sinks") {
  auto cs = std::make_shared<zest::test::capture_sink>();
  zest::logger lg("getters", cs);

  CHECK(lg.name() == "getters");
  CHECK(lg.level() == zest::level::trace);     // default
  CHECK(lg.flush_level() == zest::level::off); // default: never auto-flush
  CHECK(lg.should_log(zest::level::trace));
  REQUIRE(lg.sinks().size() == 1);
  CHECK(lg.sinks()[0] == cs);

  lg.set_level(zest::level::warn);
  lg.flush_on(zest::level::error);
  CHECK(lg.level() == zest::level::warn);
  CHECK(lg.flush_level() == zest::level::error);
  CHECK_FALSE(lg.should_log(zest::level::info));
  CHECK(lg.should_log(zest::level::error));
}
