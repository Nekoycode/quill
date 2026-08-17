#include <doctest/doctest.h>

#include <string>

#include <zest/log_msg.h>
#include <zest/pattern_formatter.h>

namespace {

zest::log_msg make_msg(zest::level lvl, std::string payload) {
  zest::log_msg m;
  m.lvl = lvl;
  m.logger_name = "test";
  m.payload = std::move(payload);
  return m;
}

std::string render(zest::pattern_formatter& pf, const zest::log_msg& msg) {
  zest::format_buffer buf;
  pf.format(msg, buf);
  return std::string(buf.view());
}

} // namespace

TEST_CASE("pattern formatter renders level and message") {
  zest::pattern_formatter pf("[%l] %v");
  CHECK(render(pf, make_msg(zest::level::warn, "hello")) == "[W] hello");
}

TEST_CASE("pattern formatter renders full level name and logger name") {
  zest::pattern_formatter pf("%n|%L|%v");
  CHECK(render(pf, make_msg(zest::level::critical, "boom")) == "test|critical|boom");
}

TEST_CASE("pattern formatter escapes percent") {
  zest::pattern_formatter pf("%% %v");
  CHECK(render(pf, make_msg(zest::level::info, "x")) == "% x");
}

TEST_CASE("pattern formatter renders source location") {
  zest::pattern_formatter pf("%s:%# %v");
  zest::log_msg m = make_msg(zest::level::info, "here");
  m.loc = zest::source_loc::current();
  const std::string out = render(pf, m);
  // "<basename>:<line> here"
  CHECK(out.find("test_pattern_formatter.cpp:") != std::string::npos);
  CHECK(out.find(" here") != std::string::npos);
}

TEST_CASE("pattern formatter renders padded time fields") {
  zest::pattern_formatter pf("%Y-%m-%d %H:%M:%S");
  const std::string out = render(pf, make_msg(zest::level::info, ""));
  // 2025-08-16 12:34:56 shape
  CHECK(out.size() == 19);
  CHECK(out[4] == '-');
  CHECK(out[7] == '-');
  CHECK(out[10] == ' ');
  CHECK(out[13] == ':');
  CHECK(out[16] == ':');
}

TEST_CASE("unknown pattern flag is preserved literally") {
  zest::pattern_formatter pf("%q %v");
  CHECK(render(pf, make_msg(zest::level::info, "x")) == "%q x");
}

TEST_CASE("trailing percent is literal") {
  zest::pattern_formatter pf("%v%");
  CHECK(render(pf, make_msg(zest::level::info, "x")) == "x%");
}

TEST_CASE("empty pattern yields empty output") {
  zest::pattern_formatter pf("");
  CHECK(render(pf, make_msg(zest::level::info, "x")).empty());
}
