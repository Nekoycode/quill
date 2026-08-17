add_executable(
  zest_tests
  test_main.cpp
  test_level.cpp
  test_pattern_formatter.cpp
  test_queue.cpp
  test_logger.cpp
  test_registry.cpp
  test_async.cpp
  test_sinks.cpp
  test_backtrace.cpp
  test_concurrency.cpp
  test_detail.cpp)

target_link_libraries(zest_tests PRIVATE zest::zest doctest)
zest_set_warnings(zest_tests)

add_test(NAME zest_tests COMMAND zest_tests)
