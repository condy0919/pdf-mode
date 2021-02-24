//! An UNREACHABLE macro.
//!
//! SPDX-License-Identifier: GPL-2.0-or-later

/// Marks that the current location is not supposed to be reachable.
/// In !NDEBUG builds, prints the message and location info to stderr.
/// In NDEBUG builds, becomes an optimizer hint that the current location is not supposed to be reachable.
///
/// Use this instead of `assert(0)`. It conveys intent more clearly and allows compilers to omit some unnecessary code.
#ifndef NDEBUG
#define YAPDF_UNREACHABLE(msg) ::yapdf::internal::unreachable(msg, __FILE__, __LINE__)
#else
#define YAPDF_UNREACHABLE(msg) __builtin_unreachable()
#endif

namespace yapdf {
namespace internal {
/// This function calls `abort()`, and prints the message to stderr.
///
/// Use `YAPDF_UNREACHABLE` macro instead of calling this function directly.
[[noreturn]] void unreachable(const char* msg, const char* file, unsigned long long line);
} // namespace internal
} // namespace yapdf
