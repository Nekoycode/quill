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
