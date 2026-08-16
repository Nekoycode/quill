#include <quill/quill.h>

#include <thread>
#include <vector>

int main() {
  auto logger = quill::file_logger_async("async", "async.log");
  logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v");

  std::vector<std::thread> threads;
  threads.reserve(4);
  for (int t = 0; t < 4; ++t) {
    threads.emplace_back([logger, t] {
      for (int i = 0; i < 100; ++i) {
        QUILL_LOGGER_INFO(logger, "thread {} message {}", t, i);
      }
    });
  }
  for (auto& th : threads) {
    th.join();
  }

  logger->flush();
  return 0;
}
