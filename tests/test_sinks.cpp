#include <doctest/doctest.h>

#include <filesystem>
#include <string>

#include <zest/zest.h>

#include "test_util.h"

namespace fs = std::filesystem;

TEST_CASE("basic_file_sink writes and truncates") {
  zest::test::temp_dir td;
  const std::string path = td.file("basic.log");

  {
    auto lg = zest::file_logger("f", path, /*truncate=*/true);
    lg->set_pattern("%v");
    lg->info("hello");
    lg->info("world");
    lg->flush();
  } // destructor closes the file

  CHECK(zest::test::read_file(path) == "hello\nworld\n");
}

TEST_CASE("basic_file_sink appends by default") {
  zest::test::temp_dir td;
  const std::string path = td.file("append.log");

  {
    auto lg = zest::file_logger("f", path, false);
    lg->set_pattern("%v");
    lg->info("first");
    lg->flush();
  }
  {
    auto lg = zest::file_logger("f", path, false);
    lg->set_pattern("%v");
    lg->info("second");
    lg->flush();
  }

  CHECK(zest::test::read_file(path) == "first\nsecond\n");
}

TEST_CASE("rotating_file_sink rotates by size") {
  zest::test::temp_dir td;
  const std::string path = td.file("rot.log");

  {
    auto sink = zest::rotating_file_sink(path, /*max_size=*/20, /*max_files=*/3);
    auto lg = zest::create_logger("rot", sink);
    lg->set_pattern("%v");
    for (int i = 0; i < 20; ++i) {
      lg->info("line number {}", i);
    }
    lg->flush();
  }

  CHECK(fs::exists(path + ".1"));
}

TEST_CASE("rolling_file_sink rotates by size and keeps max_files") {
  zest::test::temp_dir td;
  const std::string path = td.file("roll.log");

  {
    auto sink = zest::rolling_file_sink(path, /*max_size=*/20, /*max_files=*/2);
    auto lg = zest::create_logger("roll", sink);
    lg->set_pattern("%v");
    for (int i = 0; i < 20; ++i) {
      lg->info("line number {}", i);
    }
    lg->flush();
  }

  CHECK(fs::exists(path + ".1"));
  CHECK(fs::exists(path + ".2"));
  CHECK_FALSE(fs::exists(path + ".3")); // max_files=2, so nothing beyond .2
}

TEST_CASE("rolling_file_sink does not rotate an empty file for an oversized record") {
  zest::test::temp_dir td;
  const std::string path = td.file("roll_oversize.log");

  {
    auto sink = zest::rolling_file_sink(path, /*max_size=*/10, /*max_files=*/2);
    auto lg = zest::create_logger("roll_oversize", sink);
    lg->set_pattern("%v");
    // One record far larger than max_size on a fresh (empty) file.
    lg->info("this single record is way larger than the 10-byte cap");
    lg->flush();
  }

  // The oversized record is written to the base file; rotating an empty file
  // would just evict backups for nothing, so no backups must be created.
  CHECK(fs::exists(path));
  CHECK_FALSE(fs::exists(path + ".1"));
  CHECK_FALSE(fs::exists(path + ".2"));
}

TEST_CASE("daily_file_sink writes to a dated file") {
  zest::test::temp_dir td;
  const std::string base = td.file("daily");

  {
    auto sink = zest::daily_file_sink(base);
    auto lg = zest::create_logger("daily", sink);
    lg->set_pattern("%v");
    lg->info("today");
    lg->flush();
  }

  bool found = false;
  for (const auto& entry : fs::directory_iterator(td.dir())) {
    const std::string name = entry.path().filename().string();
    if (name.starts_with("daily_")) {
      found = true;
      break;
    }
  }
  CHECK(found);
}

TEST_CASE("json_sink emits structured records") {
  zest::test::temp_dir td;
  const std::string path = td.file("out.json");

  {
    auto lg = zest::create_logger("json", zest::json_sink(path));
    lg->info("hello {}", "json");
    lg->flush();
  }

  const std::string content = zest::test::read_file(path);
  CHECK(content.find("\"level\":\"info\"") != std::string::npos);
  CHECK(content.find("\"message\":\"hello json\"") != std::string::npos);
  CHECK(content.find("\"logger\":\"json\"") != std::string::npos);
  CHECK(content.find("\"time\":\"") != std::string::npos);
  CHECK(content.front() == '{');
  CHECK(content.back() == '\n');
}

TEST_CASE("json_sink escapes quotes in the message") {
  zest::test::temp_dir td;
  const std::string path = td.file("esc.json");

  {
    auto lg = zest::create_logger("json", zest::json_sink(path));
    lg->info("say \"hi\"");
    lg->flush();
  }

  const std::string content = zest::test::read_file(path);
  CHECK(content.find("\\\"hi\\\"") != std::string::npos);
}

TEST_CASE("json_sink escapes control characters and backslashes") {
  zest::test::temp_dir td;
  const std::string path = td.file("ctrl.json");

  {
    auto lg = zest::create_logger("json", zest::json_sink(path));
    lg->info("a\nb\tc\\d"); // newline, tab, backslash in the payload
    lg->flush();
  }

  // The raw control chars must be escaped in the JSON string value: newline→\n,
  // tab→\t, backslash→\\ (so the exact escaped text appears in the file).
  const std::string content = zest::test::read_file(path);
  CHECK(content.find("a\\nb\\tc\\\\d") != std::string::npos);
}

TEST_CASE("file sinks throw on fopen failure") {
  zest::test::temp_dir td;
  const std::string bad_path = td.file("missing_dir") + "/nope.log";

  CHECK_THROWS_AS(zest::basic_file_sink(bad_path), std::runtime_error);
  CHECK_THROWS_AS(zest::rolling_file_sink(bad_path, 1024, 3), std::runtime_error);
  CHECK_THROWS_AS(zest::daily_file_sink(td.file("missing_dir") + "/daily"), std::runtime_error);
}

TEST_CASE("daily_file_sink validates the rollover time") {
  zest::test::temp_dir td;
  CHECK_THROWS_AS(zest::daily_file_sink(td.file("daily"), /*rotation_hour=*/25),
                  std::invalid_argument);
  CHECK_THROWS_AS(zest::daily_file_sink(td.file("daily"), 0, /*rotation_minute=*/60),
                  std::invalid_argument);
}
