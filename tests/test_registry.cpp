#include <doctest/doctest.h>

#include <memory>

#include <zest/zest.h>

TEST_CASE("registry create/get/drop") {
  zest::drop_all();

  auto l = zest::create_logger("r1", zest::null_sink());
  REQUIRE(zest::get_logger("r1") != nullptr);
  CHECK(zest::get_logger("r1")->name() == "r1");

  zest::drop_logger("r1");
  CHECK(zest::get_logger("r1") == nullptr);
}

TEST_CASE("default logger is created lazily") {
  zest::drop_all();
  auto def = zest::default_logger();
  REQUIRE(def != nullptr);
  CHECK(def->name() == "default");
}

TEST_CASE("set_default_logger replaces the default") {
  zest::drop_all();
  auto custom = zest::create_logger("custom-default", zest::null_sink());
  zest::set_default_logger(custom);
  CHECK(zest::default_logger()->name() == "custom-default");
}
