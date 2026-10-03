#pragma once

/// Platform and compiler detection. Defines exactly one CB_PLATFORM_* and one CB_COMPILER_* macro.

#if defined(_WIN32)
#define CB_PLATFORM_WINDOWS 1
#elif defined(__APPLE__)
#define CB_PLATFORM_MACOS 1
#elif defined(__linux__)
#define CB_PLATFORM_LINUX 1
#else
#error "Carbon: unsupported platform"
#endif

#if defined(__clang__)
#define CB_COMPILER_CLANG 1
#elif defined(_MSC_VER)
#define CB_COMPILER_MSVC 1
#elif defined(__GNUC__)
#define CB_COMPILER_GCC 1
#else
#error "Carbon: unsupported compiler"
#endif

/// Defined in builds without NDEBUG.
#if !defined(NDEBUG)
#define CB_DEBUG 1
#endif

/// Stops in the debugger at the call site (or terminates when none is attached).
#if defined(_MSC_VER)
#define CB_DEBUG_BREAK() __debugbreak()
#else
#define CB_DEBUG_BREAK() __builtin_trap()
#endif
