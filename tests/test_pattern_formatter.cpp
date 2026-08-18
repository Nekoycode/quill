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

// --- zest-native brace-field syntax ({field}) ---

TEST_CASE("brace fields render level and message") {
  zest::pattern_formatter pf("{level} {msg}");
  CHECK(render(pf, make_msg(zest::level::warn, "hello")) == "warn hello");
}

TEST_CASE("brace fields render logger name and short level") {
  zest::pattern_formatter pf("[{level_short}] {logger}: {message}");
  CHECK(render(pf, make_msg(zest::level::critical, "boom")) == "[C] test: boom");
}

TEST_CASE("brace and percent syntaxes can be mixed") {
  zest::pattern_formatter pf("[%l] {logger}: %v");
  CHECK(render(pf, make_msg(zest::level::warn, "hi")) == "[W] test: hi");
}

TEST_CASE("brace escapes produce literal braces") {
  zest::pattern_formatter pf("{{msg}} {msg}");
  CHECK(render(pf, make_msg(zest::level::info, "hi")) == "{msg} hi");
}

TEST_CASE("unknown brace field is preserved verbatim") {
  zest::pattern_formatter pf("{bogus} {msg}");
  CHECK(render(pf, make_msg(zest::level::info, "hi")) == "{bogus} hi");
}

TEST_CASE("trailing lone brace is literal") {
  zest::pattern_formatter pf("{msg} {");
  CHECK(render(pf, make_msg(zest::level::info, "hi")) == "hi {");
}

TEST_CASE("composite brace fields date/time/datetime have the expected shape") {
  // The "!= all-zeros" checks guard has_time_flag_: if it were not set, format()
  // would use a zero tm_fields and print the same shape ("0000-00-00" etc.).
  {
    zest::pattern_formatter pf("{date}");
    const std::string out = render(pf, make_msg(zest::level::info, ""));
    CHECK(out.size() == 10); // YYYY-MM-DD
    CHECK(out[4] == '-');
    CHECK(out[7] == '-');
    CHECK(out != "0000-00-00");
  }
  {
    zest::pattern_formatter pf("{time}");
    const std::string out = render(pf, make_msg(zest::level::info, ""));
    CHECK(out.size() == 12); // HH:MM:SS.mmm
    CHECK(out[2] == ':');
    CHECK(out[5] == ':');
    CHECK(out[8] == '.');
    CHECK(out != "00:00:00.000");
  }
  {
    zest::pattern_formatter pf("{datetime}");
    const std::string out = render(pf, make_msg(zest::level::info, ""));
    CHECK(out.size() == 23); // date + ' ' + time
    CHECK(out[10] == ' ');
    CHECK(out != "0000-00-00 00:00:00.000");
  }
}

TEST_CASE("brace composite fields match the equivalent percent patterns") {
  const auto msg = make_msg(zest::level::info, "");
  zest::pattern_formatter brace_dt("{datetime}");
  zest::pattern_formatter pct_dt("%Y-%m-%d %H:%M:%S.%e");
  CHECK(render(brace_dt, msg) == render(pct_dt, msg));
  zest::pattern_formatter brace("{date} {time}");
  zest::pattern_formatter pct("%Y-%m-%d %H:%M:%S.%e");
  CHECK(render(brace, msg) == render(pct, msg));
}

TEST_CASE("brace source fields render basename:line, line and func") {
  zest::pattern_formatter pf("{src}|{line}|{func}|{msg}");
  zest::log_msg m = make_msg(zest::level::info, "x");
  m.loc = zest::source_loc::current();
  const std::string out = render(pf, m);
  CHECK(out.find("test_pattern_formatter.cpp:") != std::string::npos);
  CHECK(out.find('|') != std::string::npos);
  CHECK(out.find("|x") != std::string::npos);
}

TEST_CASE("brace color fields emit ANSI codes when enabled") {
  zest::pattern_formatter pf("{color_start}{msg}{color_end}");
  pf.set_color(true);
  const std::string out = render(pf, make_msg(zest::level::error, "boom"));
  CHECK(out.find("\033[") != std::string::npos);
  CHECK(out.find("boom") != std::string::npos);
}

TEST_CASE("brace parser edge cases") {
  // Verbatim-preservation / escape cases: pattern -> expected output (msg="m").
  struct Case {
    std::string pattern;
    std::string expected;
  };
  const Case cases[] = {
      {"{}", "{}"},        // empty field preserved verbatim
      {"}", "}"},          // lone close brace
      {"}}", "}"},         // }} escape
      {"a}b", "a}b"},      // lone close brace mid-pattern
      {"%{", "%{"},        // %{ -> unknown %-flag preserved
      {"x{msg", "x{msg"},  // unterminated field is literal
      {"{a{b}", "{a{b}"},  // '{' inside a field name -> preserved verbatim
      {"%% {msg}", "% m"}, // %% escape + a real field
  };
  for (const auto& c : cases) {
    zest::pattern_formatter pf(c.pattern);
    CHECK(render(pf, make_msg(zest::level::info, "m")) == c.expected);
  }
}

TEST_CASE("brace thread/pid/path fields render values") {
  zest::log_msg m = make_msg(zest::level::info, "x");
  m.loc = zest::source_loc::current();
  {
    zest::pattern_formatter pf("{thread}|{tid}");
    const std::string out = render(pf, m);
    CHECK(!out.empty());
    CHECK(out.find('|') != std::string::npos);
  }
  {
    zest::pattern_formatter pf("{pid}");
    CHECK(!render(pf, m).empty());
  }
  {
    zest::pattern_formatter pf("{path}");
    CHECK(render(pf, m).find("test_pattern_formatter.cpp") != std::string::npos);
  }
}

TEST_CASE("set_pattern recompiles and re-gates the time computation") {
  zest::pattern_formatter pf("{time}");
  const std::string t = render(pf, make_msg(zest::level::info, ""));
  CHECK(t.size() == 12);
  CHECK(t != "00:00:00.000");

  // Switch to a time-free pattern; output must not carry stale time.
  pf.set_pattern("{msg}");
  CHECK(render(pf, make_msg(zest::level::info, "m")) == "m");

  // Switch back to a time pattern; real time is computed again.
  pf.set_pattern("{time}");
  const std::string t2 = render(pf, make_msg(zest::level::info, ""));
  CHECK(t2.size() == 12);
  CHECK(t2 != "00:00:00.000");
}
