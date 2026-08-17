add_executable(bench_logger bench_logger.cpp)
target_link_libraries(bench_logger PRIVATE zest::zest)
zest_set_warnings(bench_logger)

add_executable(bench_queue bench_queue.cpp)
target_link_libraries(bench_queue PRIVATE zest::zest)
zest_set_warnings(bench_queue)

if(ZEST_BUILD_SPDLOG_BENCH)
  find_package(spdlog CONFIG REQUIRED)
  add_executable(bench_spdlog bench_spdlog.cpp)
  target_link_libraries(bench_spdlog PRIVATE zest::zest spdlog::spdlog_header_only)
  zest_set_warnings(bench_spdlog)
endif()
