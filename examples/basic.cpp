#include <quill/quill.h>

int main() {
  auto logger = quill::stdout_logger("basic");
  logger->set_pattern("%^[%H:%M:%S.%e] [%l] [%n] %v%$");

  QUILL_LOGGER_INFO(logger, "hello {}", "world");
  QUILL_LOGGER_WARN(logger, "the answer is {}", 42);
  QUILL_LOGGER_ERROR(logger, "{} + {} = {}", 1, 2, 3);

  logger->flush();
  return 0;
}
