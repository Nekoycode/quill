#include <doctest/doctest.h>

#include <quill/level.h>

TEST_CASE("level enumerator values") {
  CHECK(static_cast<int>(quill::level::trace) == 0);
  CHECK(static_cast<int>(quill::level::debug) == 1);
  CHECK(static_cast<int>(quill::level::info) == 2);
  CHECK(static_cast<int>(quill::level::warn) == 3);
  CHECK(static_cast<int>(quill::level::error) == 4);
  CHECK(static_cast<int>(quill::level::critical) == 5);
}

TEST_CASE("level to string view") {
  CHECK(quill::to_string_view(quill::level::trace) == "trace");
  CHECK(quill::to_string_view(quill::level::info) == "info");
  CHECK(quill::to_string_view(quill::level::critical) == "critical");
}

TEST_CASE("short level character") {
  CHECK(quill::short_level(quill::level::trace) == 'T');
  CHECK(quill::short_level(quill::level::warn) == 'W');
  CHECK(quill::short_level(quill::level::error) == 'E');
}
