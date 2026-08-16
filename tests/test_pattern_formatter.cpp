#include <doctest/doctest.h>

#include <string>

#include <quill/log_msg.h>
#include <quill/pattern_formatter.h>

namespace {

quill::log_msg make_msg(quill::level lvl, std::string payload) {
  quill::log_msg m;
  m.lvl = lvl;
  m.logger_name = "test";
  m.payload = std::move(payload);
  return m;
}

} // namespace

TEST_CASE("pattern formatter renders level and message") {
  quill::pattern_formatter pf("[%l] %v");
  std::string out;
  pf.format(make_msg(quill::level::warn, "hello"), out);
  CHECK(out == "[W] hello");
}

TEST_CASE("pattern formatter renders full level name and logger name") {
  quill::pattern_formatter pf("%n|%L|%v");
  std::string out;
  pf.format(make_msg(quill::level::critical, "boom"), out);
  CHECK(out == "test|critical|boom");
}

TEST_CASE("pattern formatter escapes percent") {
  quill::pattern_formatter pf("%% %v");
  std::string out;
  pf.format(make_msg(quill::level::info, "x"), out);
  CHECK(out == "% x");
}

TEST_CASE("pattern formatter renders source location") {
  quill::pattern_formatter pf("%s:%# %v");
  quill::log_msg m = make_msg(quill::level::info, "here");
  m.loc = quill::source_loc::current();
  std::string out;
  pf.format(m, out);
  // "<basename>:<line> here"
  CHECK(out.find("test_pattern_formatter.cpp:") != std::string::npos);
  CHECK(out.find(" here") != std::string::npos);
}

TEST_CASE("pattern formatter renders padded time fields") {
  quill::pattern_formatter pf("%Y-%m-%d %H:%M:%S");
  std::string out;
  pf.format(make_msg(quill::level::info, ""), out);
  // 2025-08-16 12:34:56 shape
  CHECK(out.size() == 19);
  CHECK(out[4] == '-');
  CHECK(out[7] == '-');
  CHECK(out[10] == ' ');
  CHECK(out[13] == ':');
  CHECK(out[16] == ':');
}
