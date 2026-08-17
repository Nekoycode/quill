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

TEST_CASE("level to string view") {
  CHECK(zest::to_string_view(zest::level::trace) == "trace");
  CHECK(zest::to_string_view(zest::level::info) == "info");
  CHECK(zest::to_string_view(zest::level::critical) == "critical");
}

TEST_CASE("short level character") {
  CHECK(zest::short_level(zest::level::trace) == 'T');
  CHECK(zest::short_level(zest::level::warn) == 'W');
  CHECK(zest::short_level(zest::level::error) == 'E');
}
