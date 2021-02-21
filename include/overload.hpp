//! Overload trick
//!
//! Copy from https://dev.to/tmr232/that-overloaded-trick-overloading-lambdas-in-c17
//!
//! # Examples
//!
//! ``` cpp
//! std::variant<int, const char*> var{"hello"};
//!
//! std::visit(
//!     Overload{
//!         [](const char* s) { puts(s); },
//!         [](int x) { printf("%d\n", x); },
//!     },
//!     var);
//! ```
//!
//! SPDX-License-Identifier: GPL-2.0-or-later

#ifndef YAPDF_OVERLOAD_HPP_
#define YAPDF_OVERLOAD_HPP_

namespace yapdf {
template <typename... Fns>
struct Overload : Fns... {
    using Fns::operator()...;
};

template <typename... Fns>
Overload(Fns...) -> Overload<Fns...>;
} // namespace yapdf

#endif // YAPDF_OVERLOAD_HPP_
