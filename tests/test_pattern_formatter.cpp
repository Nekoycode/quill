#include <doctest/doctest.h>

#include <sstream>
#include <string>
#include <vector>

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

TEST_CASE("pattern formatter renders the remaining time and metadata flags") {
  // %b %a (month/weekday short names), %e %f (ms/us), %I (12h hour), %t %P.
  zest::pattern_formatter pf("%b|%a|%I|%e|%f|%t|%P|%v");
  const std::string out = render(pf, make_msg(zest::level::info, "m"));

  // Split on '|' and check each field is non-empty (values are time-dependent).
  std::stringstream ss(out);
  std::vector<std::string> fields;
  for (std::string f; std::getline(ss, f, '|');) {
    fields.push_back(f);
  }
  REQUIRE(fields.size() == 8);
  CHECK(fields[0].size() == 3); // "Jan".."Dec"
  CHECK(fields[1].size() == 3); // "Sun".."Sat"
  CHECK(!fields[2].empty());    // 01..12
  CHECK(fields[3].size() == 3); // 000..999
  CHECK(fields[4].size() == 6); // 000000..999999
  CHECK(!fields[5].empty());    // thread id
  CHECK(!fields[6].empty());    // process id
  CHECK(fields[7] == "m");
}

TEST_CASE("pattern formatter renders source file and function flags") {
  zest::pattern_formatter pf("%g|%!|%v");
  zest::log_msg m = make_msg(zest::level::info, "x");
  m.loc = zest::source_loc::current();
  const std::string out = render(pf, m);
  CHECK(out.find("test_pattern_formatter.cpp|") != std::string::npos);
  CHECK(out.find("|x") != std::string::npos);
}

TEST_CASE("pattern formatter default pattern renders") {
  zest::pattern_formatter pf; // default: %^[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] [%t] %v%$
  const std::string out = render(pf, make_msg(zest::level::warn, "hello"));
  CHECK(out.find("[W] [test]") != std::string::npos);
  CHECK(out.find("hello") != std::string::npos);
}

TEST_CASE("pattern formatter color flags emit ANSI codes") {
  zest::pattern_formatter pf("%^%v%$");
  pf.set_color(true);
  const std::string out = render(pf, make_msg(zest::level::error, "boom"));
  CHECK(out.find("\033[") != std::string::npos); // some ANSI escape
  CHECK(out.find("boom") != std::string::npos);
}
