#include <doctest/doctest.h>

#include <filesystem>
#include <string>

#include <quill/quill.h>

#include "test_util.h"

namespace fs = std::filesystem;

TEST_CASE("basic_file_sink writes and truncates") {
  quill::test::temp_dir td;
  const std::string path = td.file("basic.log");

  {
    auto lg = quill::file_logger("f", path, /*truncate=*/true);
    lg->set_pattern("%v");
    lg->info("hello");
    lg->info("world");
    lg->flush();
  } // destructor closes the file

  CHECK(quill::test::read_file(path) == "hello\nworld\n");
}

TEST_CASE("basic_file_sink appends by default") {
  quill::test::temp_dir td;
  const std::string path = td.file("append.log");

  {
    auto lg = quill::file_logger("f", path, false);
    lg->set_pattern("%v");
    lg->info("first");
    lg->flush();
  }
  {
    auto lg = quill::file_logger("f", path, false);
    lg->set_pattern("%v");
    lg->info("second");
    lg->flush();
  }

  CHECK(quill::test::read_file(path) == "first\nsecond\n");
}

TEST_CASE("rotating_file_sink rotates by size") {
  quill::test::temp_dir td;
  const std::string path = td.file("rot.log");

  {
    auto sink = quill::rotating_file_sink(path, /*max_size=*/20, /*max_files=*/3);
    auto lg = quill::create_logger("rot", sink);
    lg->set_pattern("%v");
    for (int i = 0; i < 20; ++i) {
      lg->info("line number {}", i);
    }
    lg->flush();
  }

  CHECK(fs::exists(path + ".1"));
}

TEST_CASE("daily_file_sink writes to a dated file") {
  quill::test::temp_dir td;
  const std::string base = td.file("daily");

  {
    auto sink = quill::daily_file_sink(base);
    auto lg = quill::create_logger("daily", sink);
    lg->set_pattern("%v");
    lg->info("today");
    lg->flush();
  }

  bool found = false;
  for (const auto& entry : fs::directory_iterator(td.dir())) {
    const std::string name = entry.path().filename().string();
    if (name.rfind("daily_", 0) == 0) {
      found = true;
      break;
    }
  }
  CHECK(found);
}

TEST_CASE("json_sink emits structured records") {
  quill::test::temp_dir td;
  const std::string path = td.file("out.json");

  {
    auto lg = quill::create_logger("json", quill::json_sink(path));
    lg->info("hello {}", "json");
    lg->flush();
  }

  const std::string content = quill::test::read_file(path);
  CHECK(content.find("\"level\":\"info\"") != std::string::npos);
  CHECK(content.find("\"message\":\"hello json\"") != std::string::npos);
  CHECK(content.find("\"logger\":\"json\"") != std::string::npos);
  CHECK(content.find("\"time\":\"") != std::string::npos);
  CHECK(content.front() == '{');
  CHECK(content.back() == '\n');
}

TEST_CASE("json_sink escapes quotes in the message") {
  quill::test::temp_dir td;
  const std::string path = td.file("esc.json");

  {
    auto lg = quill::create_logger("json", quill::json_sink(path));
    lg->info("say \"hi\"");
    lg->flush();
  }

  const std::string content = quill::test::read_file(path);
  CHECK(content.find("\\\"hi\\\"") != std::string::npos);
}
