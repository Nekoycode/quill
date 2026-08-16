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

std::string render(quill::pattern_formatter& pf, const quill::log_msg& msg) {
  quill::format_buffer buf;
  pf.format(msg, buf);
  return std::string(buf.view());
}

} // namespace

TEST_CASE("pattern formatter renders level and message") {
  quill::pattern_formatter pf("[%l] %v");
  CHECK(render(pf, make_msg(quill::level::warn, "hello")) == "[W] hello");
}

TEST_CASE("pattern formatter renders full level name and logger name") {
  quill::pattern_formatter pf("%n|%L|%v");
  CHECK(render(pf, make_msg(quill::level::critical, "boom")) == "test|critical|boom");
}

TEST_CASE("pattern formatter escapes percent") {
  quill::pattern_formatter pf("%% %v");
  CHECK(render(pf, make_msg(quill::level::info, "x")) == "% x");
}

TEST_CASE("pattern formatter renders source location") {
  quill::pattern_formatter pf("%s:%# %v");
  quill::log_msg m = make_msg(quill::level::info, "here");
  m.loc = quill::source_loc::current();
  const std::string out = render(pf, m);
  // "<basename>:<line> here"
  CHECK(out.find("test_pattern_formatter.cpp:") != std::string::npos);
  CHECK(out.find(" here") != std::string::npos);
}

TEST_CASE("pattern formatter renders padded time fields") {
  quill::pattern_formatter pf("%Y-%m-%d %H:%M:%S");
  const std::string out = render(pf, make_msg(quill::level::info, ""));
  // 2025-08-16 12:34:56 shape
  CHECK(out.size() == 19);
  CHECK(out[4] == '-');
  CHECK(out[7] == '-');
  CHECK(out[10] == ' ');
  CHECK(out[13] == ':');
  CHECK(out[16] == ':');
}

TEST_CASE("unknown pattern flag is preserved literally") {
  quill::pattern_formatter pf("%q %v");
  CHECK(render(pf, make_msg(quill::level::info, "x")) == "%q x");
}

TEST_CASE("trailing percent is literal") {
  quill::pattern_formatter pf("%v%");
  CHECK(render(pf, make_msg(quill::level::info, "x")) == "x%");
}

TEST_CASE("empty pattern yields empty output") {
  quill::pattern_formatter pf("");
  CHECK(render(pf, make_msg(quill::level::info, "x")).empty());
}
