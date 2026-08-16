add_executable(
  quill_tests
  test_main.cpp
  test_level.cpp
  test_pattern_formatter.cpp
  test_queue.cpp
  test_logger.cpp
  test_registry.cpp
  test_async.cpp
  test_sinks.cpp
  test_backtrace.cpp
  test_concurrency.cpp)

target_link_libraries(quill_tests PRIVATE quill::quill doctest)
quill_set_warnings(quill_tests)

add_test(NAME quill_tests COMMAND quill_tests)
