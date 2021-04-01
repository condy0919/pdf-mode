//! SPDX-License-Identifier: GPL-2.0-or-later

#include "unreachable.hpp"

#include <cstdio>
#include <cstdlib>

namespace yapdf {
namespace internal {
void unreachable(const char* msg, const char* file, unsigned long long line) {
    std::fprintf(stderr, "%s\n", msg);
    std::fprintf(stderr, "UNREACHABLE executed at %s:%llu!\n", file, line);
    std::abort();
}
} // namespace internal
} // namespace yapdf
