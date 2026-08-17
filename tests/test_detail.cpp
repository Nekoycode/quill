#include <doctest/doctest.h>

#include <string>
#include <string_view>

#include <zest/detail/deferred_message.h>
#include <zest/detail/small_buffer.h>

TEST_CASE("small_buffer keeps all content when overflowing the inline buffer") {
  zest::detail::small_buffer<16> b;
  b += "aaaaaaaa";             // 8 bytes, fits inline
  b += "bbbbbbbbbbbbbbbbbbbb"; // 20 bytes -> overflows to heap (8+20 > 16)
  b += "cccc";                 // 4 bytes AFTER overflow (must go to heap)
  b.push_back('!');

  const std::string out(b.view());
  CHECK(out == "aaaaaaaabbbbbbbbbbbbbbbbbbbbcccc!");
  CHECK(out.size() == 8 + 20 + 4 + 1);
}

TEST_CASE("small_buffer stays inline when the content fits") {
  zest::detail::small_buffer<32> b;
  b += "short";
  const std::string out(b.view());
  CHECK(out == "short");
}

TEST_CASE("deferred_message move-assign from empty leaves a valid empty state") {
  auto d = zest::detail::deferred_message::make("x={}", 42);
  REQUIRE_FALSE(d.empty());

  // Move-assign an empty deferred over a non-empty one (used by the async
  // backend when it dequeues a pre-formatted record after a deferred one).
  d = zest::detail::deferred_message{};
  CHECK(d.empty());
}

TEST_CASE("deferred_message formats the captured arguments on demand") {
  auto d = zest::detail::deferred_message::make("value={} and {}", 42, "ok");
  std::string out;
  d.format_into(out);
  CHECK(out == "value=42 and ok");
}
