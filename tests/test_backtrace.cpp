#include <doctest/doctest.h>

#include <memory>

#include <zest/logger.h>

#include "capture_sink.h"

TEST_CASE("backtrace re-emits the most recent records") {
  auto cs = std::make_shared<zest::test::capture_sink>();
  zest::logger lg("bt", cs);
  lg.set_pattern("%v");
  lg.enable_backtrace(3);

  for (int i = 0; i < 5; ++i) {
    lg.info("msg {}", i);
  }
  lg.dump_backtrace();

  const auto lines = cs->lines();
  REQUIRE(lines.size() == 8); // 5 normal writes + 3 backtrace re-emits
  CHECK(lines[5] == "msg 2\n");
  CHECK(lines[6] == "msg 3\n");
  CHECK(lines[7] == "msg 4\n");
}

TEST_CASE("dump_backtrace with nothing captured adds nothing") {
  auto cs = std::make_shared<zest::test::capture_sink>();
  zest::logger lg("bt2", cs);
  lg.set_pattern("%v");

  lg.dump_backtrace();

  CHECK(cs->count() == 0);
}

TEST_CASE("disable_backtrace stops capturing") {
  auto cs = std::make_shared<zest::test::capture_sink>();
  zest::logger lg("bt3", cs);
  lg.set_pattern("%v");

  lg.enable_backtrace(2);
  lg.info("a");
  lg.disable_backtrace();
  lg.info("b");
  lg.dump_backtrace();

  // "a" (normal) + "b" (normal) + "a" (backtrace re-emit) = 3 lines
  const auto lines = cs->lines();
  REQUIRE(lines.size() == 3);
  CHECK(lines[2] == "a\n");
}

TEST_CASE("dump_backtrace flushes the sinks after replay") {
  auto fs = std::make_shared<zest::test::flush_counting_sink>();
  zest::logger lg("bt4", fs);

  lg.enable_backtrace(4);
  lg.info("one");
  lg.info("two");

  const int before = fs->flushes();
  lg.dump_backtrace();

  // The replay must end with a flush so buffered sinks persist the dump.
  CHECK(fs->flushes() == before + 1);
}
