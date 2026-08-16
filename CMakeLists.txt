cmake_minimum_required(VERSION 3.25)

project(
  quill
  VERSION 0.1.0
  DESCRIPTION "A lightweight, customizable, thread-safe C++20 logging library"
  LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# CMake >= 4.0 removed compatibility with `cmake_minimum_required(VERSION < 3.5)`.
# doctest's CMakeLists still declares an older minimum, so lower the policy floor
# while it is being fetched as a dependency.
if(CMAKE_VERSION VERSION_GREATER_EQUAL 4.0)
  set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING
      "Compatibility policy floor for third-party dependencies")
endif()

include(CompilerWarnings)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(QUILL_USE_STD_FORMAT "Use std::format (C++20) instead of the fmt library" ON)
option(QUILL_BUILD_TESTS "Build unit tests" ${PROJECT_IS_TOP_LEVEL})
option(QUILL_BUILD_EXAMPLES "Build examples" ${PROJECT_IS_TOP_LEVEL})
option(QUILL_BUILD_BENCHMARKS "Build benchmarks" OFF)
option(QUILL_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

# ---------------------------------------------------------------------------
# Dependencies (fetched only when needed; none are required at runtime)
# ---------------------------------------------------------------------------
if(NOT QUILL_USE_STD_FORMAT)
  include(FetchContent)
  FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 11.1.4
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(fmt)
endif()

if(QUILL_BUILD_TESTS)
  include(FetchContent)
  FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.11
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(doctest)
endif()

# ---------------------------------------------------------------------------
# Library target (header-only)
# ---------------------------------------------------------------------------
add_library(quill INTERFACE)
add_library(quill::quill ALIAS quill)

target_compile_features(quill INTERFACE cxx_std_20)
target_include_directories(
  quill
  INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

if(QUILL_USE_STD_FORMAT)
  target_compile_definitions(quill INTERFACE QUILL_USE_STD_FORMAT=1)
else()
  target_compile_definitions(quill INTERFACE QUILL_USE_STD_FORMAT=0)
  target_link_libraries(quill INTERFACE fmt::fmt)
endif()

# quill uses a few POSIX facilities (thread-safe local time, process id, tty
# detection). Expose them even under a strict `-std=c++20` build.
#   - Linux/BSD: _POSIX_C_SOURCE=200809L
#   - macOS:     _DARWIN_C_SOURCE (defining _POSIX_C_SOURCE alone would hide
#                fileno(), which is gated behind __DARWIN_C_FULL)
if(NOT WIN32)
  target_compile_definitions(quill INTERFACE
    $<$<PLATFORM_ID:Darwin>:_DARWIN_C_SOURCE>
    $<$<NOT:$<PLATFORM_ID:Darwin>>:_POSIX_C_SOURCE=200809L>)
endif()

# ---------------------------------------------------------------------------
# Tests / examples
# ---------------------------------------------------------------------------
if(QUILL_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(QUILL_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(QUILL_BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()

# ---------------------------------------------------------------------------
# Install / export (find_package(quill CONFIG))
# ---------------------------------------------------------------------------
install(
  DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/quill"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

install(TARGETS quill EXPORT quillTargets)
install(
  EXPORT quillTargets
  FILE quillTargets.cmake
  NAMESPACE quill::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/quill")

if(QUILL_USE_STD_FORMAT)
  set(_QUILL_USE_STD_FORMAT 1)
else()
  set(_QUILL_USE_STD_FORMAT 0)
endif()

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/quillConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/quillConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/quill")

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/quillConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMajorVersion)

install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/quillConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/quillConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/quill")
