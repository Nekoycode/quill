#include <zest/zest.h>

int main() {
  auto logger = zest::stdout_logger("basic");
  logger->set_pattern("%^[%H:%M:%S.%e] [%l] [%n] %v%$");

  ZEST_LOGGER_INFO(logger, "hello {}", "world");
  ZEST_LOGGER_WARN(logger, "the answer is {}", 42);
  ZEST_LOGGER_ERROR(logger, "{} + {} = {}", 1, 2, 3);

  logger->flush();
  return 0;
}
