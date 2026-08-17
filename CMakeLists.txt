cmake_minimum_required(VERSION 3.25)

project(
  zest
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
option(ZEST_USE_STD_FORMAT "Use std::format (C++20) instead of the fmt library" ON)
option(ZEST_BUILD_TESTS "Build unit tests" ${PROJECT_IS_TOP_LEVEL})
option(ZEST_BUILD_EXAMPLES "Build examples" ${PROJECT_IS_TOP_LEVEL})
option(ZEST_BUILD_BENCHMARKS "Build benchmarks" OFF)
option(ZEST_BUILD_SPDLOG_BENCH "Build the spdlog comparison benchmark (requires spdlog)" OFF)
option(ZEST_BUILD_DOCS "Enable the 'docs' target (requires Doxygen)" OFF)
option(ZEST_ENABLE_CLANG_TIDY "Run clang-tidy during compilation" OFF)
option(ZEST_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

# Static analysis: clang-tidy must be wired into CMAKE_CXX_CLANG_TIDY before any
# target is created for it to take effect.
if(ZEST_ENABLE_CLANG_TIDY)
  find_program(ZEST_CLANG_TIDY_EXE NAMES clang-tidy)
  if(ZEST_CLANG_TIDY_EXE)
    set(CMAKE_CXX_CLANG_TIDY "${ZEST_CLANG_TIDY_EXE}")
  else()
    message(WARNING "ZEST_ENABLE_CLANG_TIDY is ON but clang-tidy was not found")
  endif()
endif()

# ---------------------------------------------------------------------------
# Dependencies (fetched only when needed; none are required at runtime)
# ---------------------------------------------------------------------------
if(NOT ZEST_USE_STD_FORMAT)
  include(FetchContent)
  FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 11.1.4
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(fmt)
endif()

if(ZEST_BUILD_TESTS)
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
add_library(zest INTERFACE)
add_library(zest::zest ALIAS zest)

target_compile_features(zest INTERFACE cxx_std_20)
target_include_directories(
  zest
  INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

if(ZEST_USE_STD_FORMAT)
  target_compile_definitions(zest INTERFACE ZEST_USE_STD_FORMAT=1)
else()
  target_compile_definitions(zest INTERFACE ZEST_USE_STD_FORMAT=0)
  target_link_libraries(zest INTERFACE fmt::fmt)
endif()

# zest uses a few POSIX facilities (thread-safe local time, process id, tty
# detection). Expose them even under a strict `-std=c++20` build.
#   - Linux/BSD: _POSIX_C_SOURCE=200809L
#   - macOS:     _DARWIN_C_SOURCE (defining _POSIX_C_SOURCE alone would hide
#                fileno(), which is gated behind __DARWIN_C_FULL)
if(NOT WIN32)
  target_compile_definitions(zest INTERFACE
    $<$<PLATFORM_ID:Darwin>:_DARWIN_C_SOURCE>
    $<$<NOT:$<PLATFORM_ID:Darwin>>:_POSIX_C_SOURCE=200809L>)
endif()

# ---------------------------------------------------------------------------
# Tests / examples
# ---------------------------------------------------------------------------
if(ZEST_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(ZEST_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(ZEST_BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()

# ---------------------------------------------------------------------------
# Developer conveniences: clang-format and Doxygen targets (optional tools)
# ---------------------------------------------------------------------------
find_program(ZEST_CLANG_FORMAT_EXE NAMES clang-format)
if(ZEST_CLANG_FORMAT_EXE)
  file(GLOB_RECURSE ZEST_FORMAT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/*.h")

  add_custom_target(
    format
    COMMAND "${ZEST_CLANG_FORMAT_EXE}" -i ${ZEST_FORMAT_SOURCES}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Formatting sources with clang-format")

  add_custom_target(
    check-format
    COMMAND "${ZEST_CLANG_FORMAT_EXE}" --dry-run --Werror ${ZEST_FORMAT_SOURCES}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Checking formatting with clang-format")
endif()

if(ZEST_BUILD_DOCS)
  find_program(ZEST_DOXYGEN_EXE NAMES doxygen)
  if(ZEST_DOXYGEN_EXE)
    add_custom_target(
      docs
      COMMAND "${ZEST_DOXYGEN_EXE}" "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile"
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      COMMENT "Generating API documentation with Doxygen")
  else()
    message(WARNING "ZEST_BUILD_DOCS is ON but Doxygen was not found")
  endif()
endif()

# ---------------------------------------------------------------------------
# Install / export (find_package(zest CONFIG))
# ---------------------------------------------------------------------------
install(
  DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/zest"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

install(TARGETS zest EXPORT zestTargets)
install(
  EXPORT zestTargets
  FILE zestTargets.cmake
  NAMESPACE zest::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/zest")

if(ZEST_USE_STD_FORMAT)
  set(_ZEST_USE_STD_FORMAT 1)
else()
  set(_ZEST_USE_STD_FORMAT 0)
endif()

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/zestConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/zestConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/zest")

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/zestConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMajorVersion)

install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/zestConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/zestConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/zest")
