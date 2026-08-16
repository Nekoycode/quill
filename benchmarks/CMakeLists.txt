add_executable(bench_logger bench_logger.cpp)
target_link_libraries(bench_logger PRIVATE quill::quill)
quill_set_warnings(bench_logger)

add_executable(bench_queue bench_queue.cpp)
target_link_libraries(bench_queue PRIVATE quill::quill)
quill_set_warnings(bench_queue)

if(QUILL_BUILD_SPDLOG_BENCH)
  find_package(spdlog CONFIG REQUIRED)
  add_executable(bench_spdlog bench_spdlog.cpp)
  target_link_libraries(bench_spdlog PRIVATE quill::quill spdlog::spdlog_header_only)
  quill_set_warnings(bench_spdlog)
endif()
