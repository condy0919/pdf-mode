#include "bridge.hpp"

namespace yapdf {
namespace emacs {
Value::VectorProxy& Value::VectorProxy::operator=(Value v) noexcept {
    YAPDF_EMACS_APPLY(env_, vec_set, val_, idx_, v.native());
    return *this;
}

Value::VectorProxy::operator Value() const noexcept {
    return Value(YAPDF_EMACS_APPLY(env_, vec_get, val_, idx_), env_);
}

Value Value::typeOf() const noexcept {
    return Value(YAPDF_EMACS_APPLY(env_, type_of, val_), env_);
}

Expected<std::string, Error> Value::name() const noexcept {
    return env_.call("symbol-name", *this).andThen([](Value v) { return v.as<Value::Type::String>(); });
}

std::size_t Value::size() const noexcept {
    return YAPDF_EMACS_APPLY(env_, vec_size, val_);
}

Value::VectorProxy Value::operator[](std::size_t idx) noexcept {
    return Value::VectorProxy(idx, val_, env_);
}

void Value::reset(void* p) noexcept {
    YAPDF_EMACS_APPLY(env_, set_user_ptr, val_, p);
}

auto Value::finalizer() const noexcept -> void (*)(void*) EMACS_NOEXCEPT {
    return YAPDF_EMACS_APPLY(env_, get_user_finalizer, val_);
}

void Value::finalizer(void (*fin)(void*) EMACS_NOEXCEPT) noexcept {
    YAPDF_EMACS_APPLY(env_, set_user_finalizer, val_, fin);
}

#if EMACS_MAJOR_VERSION >= 28
Expected<Void, Error> Value::interactive(const char* spec) noexcept {
    const Value s = YAPDF_TRY(env_.make<Value::Type::String>(spec));
    return YAPDF_EMACS_APPLY_CHECK(env_, make_interactive, val_, s.native());
}
#endif

Value::operator bool() const noexcept {
    return YAPDF_EMACS_APPLY(env_, is_not_nil, val_);
}

bool Value::operator==(const Value& rhs) const noexcept {
    return YAPDF_EMACS_APPLY(env_, eq, val_, rhs.val_);
}
} // namespace emacs
} // namespace yapdf
