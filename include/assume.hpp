//! The assume macro
//!
//! # Brief
//!
//! The boolean argument to this function is defined to be true. The optimizer may analyze the form of the expression
//! provided as the argument and deduce from that information used to optimize the program. If the condition is violated
//! during execution, the behavior is undefined. The argument itself is never evaluated, so any side effects of the
//! expression will be discarded.
//!
//! # Examples
//!
//! ``` cpp
//! int divide_by_32(int x) {
//!     YAPDF_ASSUME(x >= 0);
//!     return x / 32;
//! }
//! ```
//!
//! # Reference
//!
//! http://wg21.link/p1774r0
//! https://clang.llvm.org/docs/LanguageExtensions.html#builtin-assume
//!
//! SPDX-License-Identifier: GPL-2.0-or-later

#ifndef YAPDF_ASSUME_HPP_
#define YAPDF_ASSUME_HPP_

#if defined(__clang__)
#define YAPDF_ASSUME(exp) __builtin_assume((exp))
#elif defined(__GNUC__)
#define YAPDF_ASSUME(exp) (void)((exp) ? 0 : (__builtin_unreachable(), 0))
#else
#define YAPDF_ASSUME(exp)
#endif

#endif // YAPDF_ASSUME_HPP_
