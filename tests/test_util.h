#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace quill::test {

// RAII temporary directory, removed on destruction.
class temp_dir {
public:
  temp_dir() {
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() / ("quill_test_" + std::to_string(suffix));
    std::filesystem::create_directories(path_);
  }

  ~temp_dir() {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }

  const std::filesystem::path& dir() const noexcept { return path_; }

  std::string file(const std::string& name) const { return (path_ / name).string(); }

private:
  std::filesystem::path path_;
};

inline std::string read_file(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

} // namespace quill::test
