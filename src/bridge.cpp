#include "bridge.hpp"

namespace yapdf {
namespace emacs {
Value::operator bool() const noexcept {
    return YAPDF_EMACS_APPLY(env_.native(), is_not_nil, val_);
}

bool Value::operator==(const Value& rhs) const noexcept {
    return YAPDF_EMACS_APPLY(env_.native(), eq, val_, rhs.val_);
}
} // namespace emacs
} // namespace yapdf
