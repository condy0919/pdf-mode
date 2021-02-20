#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include "bridge.hpp"

emacs_env* env;
int plugin_is_GPL_compatible;

int emacs_module_init(struct emacs_runtime* runtime) EMACS_NOEXCEPT {
    doctest::Context ctx;

    // leak the emacs_env pointer
    env = runtime->get_environment(runtime);
    return ctx.run();
}

TEST_CASE("Conversion Between Lisp and Module Values") {
    yapdf::emacs::Env e(env);
    using yapdf::emacs::Value;

    SUBCASE("Integer") {
        for (int i : {1, 42, 65536, -1}) {
            // c++ -> lisp
            auto val = e.make<Value::Type::Int>(i).value();

            // (type-of 1) -> 'integer
            REQUIRE_EQ(val.typeOf(), e.intern("integer").value());

            // lisp -> c++
            REQUIRE_EQ(val.as<Value::Type::Int>().value(), i);
        }
    }

    SUBCASE("BigInteger") {

    }

    SUBCASE("Float") {

    }

    SUBCASE("Time") {

    }

    SUBCASE("String") {

    }

    SUBCASE("Vector") {

    }
}
