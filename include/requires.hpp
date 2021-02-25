//! The REQUIRES facility macro.
//!
//! SPDX-License-Identifier: GPL-2.0-or-later

#ifndef YAPDF_REQUIRES_HPP_
#define YAPDF_REQUIRES_HPP_

#include <type_traits>

#define YAPDF_REQUIRES(...) std::enable_if_t<static_cast<bool>(__VA_ARGS__), int> = __LINE__

#endif // YAPDF_REQUIRES_HPP_
