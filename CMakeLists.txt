cmake_minimum_required(VERSION 3.25)

project(
  zest
  VERSION 0.1.0
  DESCRIPTION "A lightweight, customizable, thread-safe C++20 logging library"
  LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# zest's headers use std::thread/std::jthread/std::mutex/std::condition_variable.
# Modern glibc (>= 2.34) folds pthreads into libc, but older glibc, musl and
# static linking still require -pthread for consumers of this header-only lib.
find_package(Threads REQUIRED)

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
option(ZEST_BUILD_MODULE "Build the C++20 module target zest_module (import zest); requires CMake >= 3.28, Ninja, gcc>=16/clang>=17" OFF)
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
target_link_libraries(zest INTERFACE Threads::Threads)
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
# Optional C++20 module target (import zest)
#
# zest is header-only; this additionally compiles src/zest.cppm as a real
# module unit so consumers can `import zest;`. Off by default. Requires CMake
# >= 3.28, a Ninja generator, and a module-capable compiler (gcc >= 16 /
# clang >= 17). The header-only `zest` target is completely unaffected.
# ---------------------------------------------------------------------------
if(ZEST_BUILD_MODULE)
  if(CMAKE_VERSION VERSION_LESS 3.28)
    message(FATAL_ERROR
      "ZEST_BUILD_MODULE=ON requires CMake >= 3.28 (C++20 module support); "
      "found ${CMAKE_VERSION}. (CMake 3.26/3.27 only had an experimental "
      "module API, which this project does not support.)")
  endif()
  if(NOT CMAKE_GENERATOR MATCHES "Ninja|Visual Studio")
    message(FATAL_ERROR
      "ZEST_BUILD_MODULE requires the Ninja (or Visual Studio) generator for "
      "C++20 module dependency scanning; found '${CMAKE_GENERATOR}'.")
  endif()
  # GCC < 16 cannot make placement new (operator new(size_t, void*)) reachable
  # to module importers (GCC bug 101140), and zest's deferred-formatting SBO
  # uses placement new in templates that consumers instantiate. Clang < 17 has
  # comparably immature module dependency scanning. (MSVC modules are untested.)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 16)
    message(FATAL_ERROR
      "ZEST_BUILD_MODULE with GCC requires GCC >= 16 (found "
      "${CMAKE_CXX_COMPILER_VERSION}): GCC 15 and earlier cannot propagate "
      "placement new through a module BMI (GCC bug 101140). Use GCC >= 16 or "
      "Clang >= 17, e.g.  CXX=clang++ cmake --preset module")
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 17)
    message(FATAL_ERROR
      "ZEST_BUILD_MODULE with Clang requires Clang >= 17 (found "
      "${CMAKE_CXX_COMPILER_VERSION}).")
  endif()
  cmake_policy(SET CMP0155 NEW) # scan C++ sources for module imports

  # Pin STATIC: a module BMI must be rebuilt per consumer anyway, and static is
  # the verified form (prevents BUILD_SHARED_LIBS flipping this to SHARED).
  add_library(zest_module STATIC)
  add_library(zest::module ALIAS zest_module)
  target_sources(
    zest_module
    PUBLIC FILE_SET CXX_MODULES BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
           FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/zest.cppm")
  target_compile_features(zest_module PUBLIC cxx_std_20)
  # The module's global module fragment textually includes <zest/zest.h>.
  target_include_directories(zest_module PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
  zest_set_warnings(zest_module)

  # Keep the format backend identical to the header-only target. Module
  # consumers instantiate the exported templates, so they must agree on this
  # macro -> PUBLIC visibility.
  if(ZEST_USE_STD_FORMAT)
    target_compile_definitions(zest_module PUBLIC ZEST_USE_STD_FORMAT=1)
  else()
    target_compile_definitions(zest_module PUBLIC ZEST_USE_STD_FORMAT=0)
    target_link_libraries(zest_module PUBLIC fmt::fmt)
  endif()

  if(NOT WIN32)
    target_compile_definitions(
      zest_module PRIVATE $<$<PLATFORM_ID:Darwin>:_DARWIN_C_SOURCE>
                          $<$<NOT:$<PLATFORM_ID:Darwin>>:_POSIX_C_SOURCE=200809L>)
  endif()
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
