//! The regular `void` type.
//!
//! It's more like the unit type in functional programming language. Introduce here for facilitating template
//! meta-programming.
//!
//! Visit http://www.open-std.org/JTC1/SC22/WG21/docs/papers/2016/p0146r1.html for more information.
//!
//! SPDX-License-Identifier: GPL-2.0-or-later


#ifndef YAPDF_VOID_HPP_
#define YAPDF_VOID_HPP_

#include <variant>

namespace yapdf {
using Void = std::monostate;
} // namespace yapdf

#endif // YAPDF_VOID_HPP_
