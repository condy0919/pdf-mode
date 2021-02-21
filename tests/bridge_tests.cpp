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

        // integer -> float will signal an error
        auto val = e.make<Value::Type::Int>(1).value();
        auto err = val.as<Value::Type::Float>().error();
        REQUIRE_EQ(err.status(), yapdf::emacs::FuncallExit::Signal);
        REQUIRE_EQ(err.symbol(), e.intern("wrong-type-argument").value());
        // TODO REQUIRE_EQ(err.data(), val);
    }

    SUBCASE("Float") {
        for (double f : {0.0, 0.618, 3.14}) {
            // c++ -> lisp
            auto val = e.make<Value::Type::Float>(f).value();

            // (type-of 0.618) -> 'float
            REQUIRE_EQ(val.typeOf(), e.intern("float").value());

            // lisp -> c++
            REQUIRE_EQ(val.as<Value::Type::Float>().value(), doctest::Approx(f));
        }
    }

    SUBCASE("Time") {

    }

    SUBCASE("String") {
        // c++ -> lisp
        auto val = e.make<Value::Type::String>("foobarbaz").value();

        // (type-of "foobarbaz") -> 'string
        REQUIRE_EQ(val.typeOf(), e.intern("string").value());

        // lisp -> c++
        REQUIRE_EQ(val.as<Value::Type::String>().value(), "foobarbaz");

        // a string with two '\0's
        auto zeros = e.make<Value::Type::String>("\0\0", 2).value();
        REQUIRE_EQ(zeros.as<Value::Type::String>().value(), std::string("\0\0", 2));

        // an empty string
        auto empty = e.make<Value::Type::String>("").value();
        REQUIRE_EQ(empty.as<Value::Type::String>().value(), "");
    }

    SUBCASE("Vector") {

    }
}

TEST_CASE("Funcall") {
    yapdf::emacs::Env e(env);
    using yapdf::emacs::Value;

    auto val = e.call("length", "abc");
    REQUIRE_EQ(val.value().as<Value::Type::Int>().value(), 3);
}
