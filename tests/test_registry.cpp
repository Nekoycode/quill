#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

#include <zest/zest.h>

namespace {

// A sink that counts how many times flush() is called.
class flush_counting_sink final : public zest::sinks::sink {
public:
  void flush() override { flushes_.fetch_add(1, std::memory_order_relaxed); }
  int flushes() const { return flushes_.load(std::memory_order_relaxed); }

protected:
  void write_output([[maybe_unused]] std::string_view line) override {}

private:
  std::atomic<int> flushes_{0};
};

} // namespace

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
  auto sink = std::make_shared<flush_counting_sink>();
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
  auto sink = std::make_shared<flush_counting_sink>();
  zest::create_logger("periodic-stop", sink);

  zest::flush_every(std::chrono::milliseconds{15});
  std::this_thread::sleep_for(std::chrono::milliseconds{60});
  zest::flush_every(std::chrono::milliseconds{0}); // stop (joins the thread)

  const int after_stop = sink->flushes();
  std::this_thread::sleep_for(std::chrono::milliseconds{60});
  CHECK(sink->flushes() == after_stop); // no further flushes once stopped
}
