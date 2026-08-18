#pragma once

#include <cstdint>
#include <string_view>

// ---------------------------------------------------------------------------
// Version
// ---------------------------------------------------------------------------
#define ZEST_VERSION_MAJOR 0
#define ZEST_VERSION_MINOR 1
#define ZEST_VERSION_PATCH 0
#define ZEST_VERSION_STRING "0.1.0"

// ---------------------------------------------------------------------------
// Formatting backend selection
// ---------------------------------------------------------------------------
#if !defined(ZEST_USE_STD_FORMAT)
#define ZEST_USE_STD_FORMAT 1
#endif

// ---------------------------------------------------------------------------
// Linkage
//
// zest is header-only by default: all non-template free functions are marked
// `inline`. Defining ZEST_COMPILED_LIB switches to an out-of-line (compiled)
// model, which is reserved for a future compiled build mode.
// ---------------------------------------------------------------------------
#if defined(ZEST_COMPILED_LIB)
#if defined(_WIN32) && defined(ZEST_SHARED_LIB)
#ifdef ZEST_BUILDING_LIBRARY
#define ZEST_API __declspec(dllexport)
#else
#define ZEST_API __declspec(dllimport)
#endif
#else
#define ZEST_API
#endif
#else
#define ZEST_API inline
#endif

#define ZEST_INLINE inline
#define ZEST_NODISCARD [[nodiscard]]

namespace zest {

inline constexpr int version_major = ZEST_VERSION_MAJOR;
inline constexpr int version_minor = ZEST_VERSION_MINOR;
inline constexpr int version_patch = ZEST_VERSION_PATCH;
// A string_view constant so the version is reachable from the C++20 module too
// (the ZEST_VERSION_STRING macro cannot be exported by a module).
inline constexpr std::string_view version_string = ZEST_VERSION_STRING;

} // namespace zest
