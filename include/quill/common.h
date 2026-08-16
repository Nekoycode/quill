#pragma once

#include <cstdint>

// ---------------------------------------------------------------------------
// Version
// ---------------------------------------------------------------------------
#define QUILL_VERSION_MAJOR 0
#define QUILL_VERSION_MINOR 1
#define QUILL_VERSION_PATCH 0
#define QUILL_VERSION_STRING "0.1.0"

// ---------------------------------------------------------------------------
// Formatting backend selection
// ---------------------------------------------------------------------------
#if !defined(QUILL_USE_STD_FORMAT)
#define QUILL_USE_STD_FORMAT 1
#endif

// ---------------------------------------------------------------------------
// Linkage
//
// quill is header-only by default: all non-template free functions are marked
// `inline`. Defining QUILL_COMPILED_LIB switches to an out-of-line (compiled)
// model, which is reserved for a future compiled build mode.
// ---------------------------------------------------------------------------
#if defined(QUILL_COMPILED_LIB)
#if defined(_WIN32) && defined(QUILL_SHARED_LIB)
#ifdef QUILL_BUILDING_LIBRARY
#define QUILL_API __declspec(dllexport)
#else
#define QUILL_API __declspec(dllimport)
#endif
#else
#define QUILL_API
#endif
#else
#define QUILL_API inline
#endif

#define QUILL_INLINE inline
#define QUILL_NODISCARD [[nodiscard]]

namespace quill {

inline constexpr int version_major = QUILL_VERSION_MAJOR;
inline constexpr int version_minor = QUILL_VERSION_MINOR;
inline constexpr int version_patch = QUILL_VERSION_PATCH;

} // namespace quill
