add_executable(bench_logger bench_logger.cpp)
target_link_libraries(bench_logger PRIVATE quill::quill)
quill_set_warnings(bench_logger)

add_executable(bench_queue bench_queue.cpp)
target_link_libraries(bench_queue PRIVATE quill::quill)
quill_set_warnings(bench_queue)
