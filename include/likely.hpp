//! The wellknown likely/unlikely macros
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
