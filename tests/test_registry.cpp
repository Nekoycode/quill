#include <doctest/doctest.h>

#include <memory>

#include <quill/quill.h>

TEST_CASE("registry create/get/drop") {
  quill::drop_all();

  auto l = quill::create_logger("r1", quill::null_sink());
  REQUIRE(quill::get_logger("r1") != nullptr);
  CHECK(quill::get_logger("r1")->name() == "r1");

  quill::drop_logger("r1");
  CHECK(quill::get_logger("r1") == nullptr);
}

TEST_CASE("default logger is created lazily") {
  quill::drop_all();
  auto def = quill::default_logger();
  REQUIRE(def != nullptr);
  CHECK(def->name() == "default");
}

TEST_CASE("set_default_logger replaces the default") {
  quill::drop_all();
  auto custom = quill::create_logger("custom-default", quill::null_sink());
  quill::set_default_logger(custom);
  CHECK(quill::default_logger()->name() == "custom-default");
}
