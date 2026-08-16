#include <quill/quill.h>

#include <iostream>
#include <mutex>
#include <string>
#include <vector>

// Minimal custom sink that collects lines into a vector instead of writing
// them to an I/O destination.
class vector_sink final : public quill::sinks::sink {
public:
  void write(const std::string& payload) override {
    std::lock_guard<std::mutex> lock(mutex_);
    lines_.push_back(payload);
  }

  void flush() override {}

  void print_all() const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& line : lines_) {
      std::cout << line;
    }
  }

private:
  std::vector<std::string> lines_;
};

int main() {
  auto sink = std::make_shared<vector_sink>();
  auto logger = quill::create_logger("custom", sink);
  logger->set_pattern("%L: %v");

  QUILL_LOGGER_INFO(logger, "collected {}", 1);
  QUILL_LOGGER_ERROR(logger, "collected {}", 2);

  logger->flush();
  sink->print_all();
  return 0;
}
