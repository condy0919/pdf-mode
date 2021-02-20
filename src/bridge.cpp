#include "bridge.hpp"

namespace yapdf {
namespace emacs {
Value Value::typeOf() const noexcept {
    return Value(YAPDF_EMACS_APPLY(env_, type_of, val_), env_);
}

Value::operator bool() const noexcept {
    return YAPDF_EMACS_APPLY(env_, is_not_nil, val_);
}

bool Value::operator==(const Value& rhs) const noexcept {
    return YAPDF_EMACS_APPLY(env_, eq, val_, rhs.val_);
}
} // namespace emacs
} // namespace yapdf
