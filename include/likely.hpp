//! The wellknown likely/unlikely macros
//!
//! The use of the `likely` macro is intended to allow compilers to optimize for the good path. The use of the
//! `unlikely` macro is intended to allow compilers to optimize for bad path.
//!
//! All allows compilers to optimize code layout to be more cache friendly. And all require a `bool` value or else
//! compile error.
//!
//! Excessive usage of either of these macros is liable to result in performance degradation.
//!
//! # Examples
//!
//! Basic use:
//!
//! ```
//! if (YAPDF_LIKELY(foo())) {
//!     // do sth...
//! }
//! ```
//!
//! # Reference
//!
//! http://wg21.link/p0479r5
//! https://lwn.net/Articles/255364/
//!
//! SPDX-License-Identifier: GPL-2.0-or-later

#ifndef YAPDF_LIKELY_HPP_
#define YAPDF_LIKELY_HPP_

#ifdef __GNUC__
#define YAPDF_LIKELY(x) (__builtin_expect((x), 1))
#define YAPDF_UNLIKELY(x) (__builtin_expect((x), 0))
#else
#define YAPDF_LIKELY(x) (x)
#define YAPDF_UNLIKELY(x) (x)
#endif

#endif // YAPDF_LIKELY_HPP_
