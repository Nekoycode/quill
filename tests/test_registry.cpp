#include <doctest/doctest.h>

#include <chrono>
#include <memory>
#include <thread>

#include <zest/zest.h>

#include "capture_sink.h"

TEST_CASE("registry create/get/drop") {
  zest::drop_all();

  auto l = zest::create_logger("r1", zest::null_sink());
  REQUIRE(zest::get_logger("r1") != nullptr);
  CHECK(zest::get_logger("r1")->name() == "r1");

  zest::drop_logger("r1");
  CHECK(zest::get_logger("r1") == nullptr);
}

TEST_CASE("default logger is created lazily") {
  zest::drop_all();
  auto def = zest::default_logger();
  REQUIRE(def != nullptr);
  CHECK(def->name() == "default");
}

TEST_CASE("set_default_logger replaces the default") {
  zest::drop_all();
  auto custom = zest::create_logger("custom-default", zest::null_sink());
  zest::set_default_logger(custom);
  CHECK(zest::default_logger()->name() == "custom-default");
}

TEST_CASE("flush_every periodically flushes registered loggers") {
  zest::drop_all();
  auto sink = std::make_shared<zest::test::flush_counting_sink>();
  zest::create_logger("periodic", sink);

  // NOTE: real (wall-clock) durations — the interval must not be scaled, since
  // the flusher thread wakes on a steady-clock timeout regardless of the sanitizer.
  zest::flush_every(std::chrono::milliseconds{20});
  std::this_thread::sleep_for(std::chrono::milliseconds{150});
  zest::flush_every(std::chrono::milliseconds{0}); // stop the flusher

  CHECK(sink->flushes() >= 1);
}

TEST_CASE("flush_every stops firing after a non-positive interval") {
  zest::drop_all();
  auto sink = std::make_shared<zest::test::flush_counting_sink>();
  zest::create_logger("periodic-stop", sink);

  zest::flush_every(std::chrono::milliseconds{15});
  std::this_thread::sleep_for(std::chrono::milliseconds{60});
  zest::flush_every(std::chrono::milliseconds{0}); // stop (joins the thread)

  const int after_stop = sink->flushes();
  std::this_thread::sleep_for(std::chrono::milliseconds{60});
  CHECK(sink->flushes() == after_stop); // no further flushes once stopped
}

TEST_CASE("flush_every stops on shutdown") {
  zest::drop_all();
  auto sink = std::make_shared<zest::test::flush_counting_sink>();
  zest::create_logger("periodic-shutdown", sink);

  zest::flush_every(std::chrono::milliseconds{15});
  std::this_thread::sleep_for(std::chrono::milliseconds{60});
  zest::shutdown(); // shutdown must stop the periodic flusher

  const int after_shutdown = sink->flushes();
  std::this_thread::sleep_for(std::chrono::milliseconds{60});
  CHECK(sink->flushes() == after_shutdown); // no further flushes after shutdown
}

TEST_CASE("re-registering the same name replaces the entry") {
  zest::drop_all();
  auto a = zest::create_logger("dup", zest::null_sink());
  auto b = zest::create_logger("dup", zest::null_sink());
  CHECK(zest::get_logger("dup") == b); // the later registration wins
  CHECK(zest::get_logger("dup") != a);
  zest::drop_all();
}

TEST_CASE("dropping the default logger resets it for lazy recreation") {
  zest::drop_all();
  auto first = zest::default_logger(); // lazily created "default"
  REQUIRE(first != nullptr);
  REQUIRE(first->name() == "default");

  zest::drop_logger("default");
  CHECK(zest::get_logger("default") == nullptr);

  auto second = zest::default_logger(); // recreated fresh on demand
  CHECK(second->name() == "default");
  CHECK(second != first);
  zest::drop_all();
}

TEST_CASE("shutdown clears every registered logger and the default") {
  zest::drop_all();
  zest::create_logger("s1", zest::null_sink());
  zest::create_logger("s2", zest::null_sink());
  REQUIRE(zest::get_logger("s1") != nullptr);
  REQUIRE(zest::get_logger("s2") != nullptr);

  zest::shutdown();
  CHECK(zest::get_logger("s1") == nullptr);
  CHECK(zest::get_logger("s2") == nullptr);
}
