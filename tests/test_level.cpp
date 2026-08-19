#include <doctest/doctest.h>

#include <ostream>

#include <zest/level.h>

TEST_CASE("level enumerator values") {
  CHECK(static_cast<int>(zest::level::trace) == 0);
  CHECK(static_cast<int>(zest::level::debug) == 1);
  CHECK(static_cast<int>(zest::level::info) == 2);
  CHECK(static_cast<int>(zest::level::warn) == 3);
  CHECK(static_cast<int>(zest::level::error) == 4);
  CHECK(static_cast<int>(zest::level::critical) == 5);
}

TEST_CASE("level to string view covers every level and invalid input") {
  CHECK(zest::to_string_view(zest::level::trace) == "trace");
  CHECK(zest::to_string_view(zest::level::debug) == "debug");
  CHECK(zest::to_string_view(zest::level::info) == "info");
  CHECK(zest::to_string_view(zest::level::warn) == "warn");
  CHECK(zest::to_string_view(zest::level::error) == "error");
  CHECK(zest::to_string_view(zest::level::critical) == "critical");
  CHECK(zest::to_string_view(zest::level::off) == "off");
  CHECK(zest::to_string_view(zest::level::n_levels) == "???");
}

TEST_CASE("short level covers every level and invalid input") {
  CHECK(zest::short_level(zest::level::trace) == 'T');
  CHECK(zest::short_level(zest::level::debug) == 'D');
  CHECK(zest::short_level(zest::level::info) == 'I');
  CHECK(zest::short_level(zest::level::warn) == 'W');
  CHECK(zest::short_level(zest::level::error) == 'E');
  CHECK(zest::short_level(zest::level::critical) == 'C');
  CHECK(zest::short_level(zest::level::n_levels) == '?');
}

TEST_CASE("from_string_view parses canonical names and the warning alias") {
  auto as_int = [](std::string_view s) {
    auto r = zest::from_string_view(s);
    return r ? static_cast<int>(*r) : -1;
  };
  CHECK(as_int("trace") == 0);
  CHECK(as_int("debug") == 1);
  CHECK(as_int("info") == 2);
  CHECK(as_int("warn") == 3);
  CHECK(as_int("warning") == 3);
  CHECK(as_int("error") == 4);
  CHECK(as_int("critical") == 5);
  CHECK(as_int("off") == 6);
}

TEST_CASE("from_string_view rejects unknown input") {
  CHECK(!zest::from_string_view("").has_value());
  CHECK(!zest::from_string_view("verbose").has_value());
  CHECK(!zest::from_string_view("INFO").has_value()); // exact lowercase match by design
  CHECK(!zest::from_string_view("n_levels").has_value());
}

TEST_CASE("from_string_view is the inverse of to_string_view") {
  for (zest::level lvl :
       {zest::level::trace, zest::level::debug, zest::level::info, zest::level::warn,
        zest::level::error, zest::level::critical, zest::level::off}) {
    // value_or avoids an unchecked optional access (clang-tidy
    // bugprone-unchecked-optional-access); a parse failure yields n_levels,
    // which never equals lvl, so the CHECK still catches a broken parse.
    CHECK(static_cast<int>(
              zest::from_string_view(zest::to_string_view(lvl)).value_or(zest::level::n_levels)) ==
          static_cast<int>(lvl));
  }
}
