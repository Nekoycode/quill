#include <zest/zest.h>

#include <iostream>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

// Minimal custom sink that collects lines into a vector instead of writing
// them to an I/O destination.
class vector_sink final : public zest::sinks::sink {
public:
  void flush() override {}

  void print_all() const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& line : lines_) {
      std::cout << line;
    }
  }

protected:
  void write_output(std::string_view line) override {
    std::lock_guard<std::mutex> lock(mutex_);
    lines_.emplace_back(line);
  }

private:
  std::vector<std::string> lines_;
};

int main() {
  auto sink = std::make_shared<vector_sink>();
  auto logger = zest::create_logger("custom", sink);
  logger->set_pattern("%L: %v");

  ZEST_LOGGER_INFO(logger, "collected {}", 1);
  ZEST_LOGGER_ERROR(logger, "collected {}", 2);

  logger->flush();
  sink->print_all();
  return 0;
}
