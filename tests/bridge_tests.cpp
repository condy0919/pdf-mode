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
    yapdf::emacs::Env emacs(env);

    SUBCASE("Integer") {
        // REQUIRE_EQ(emacs.makeInteger(-1).asInteger(), -1);
        // REQUIRE_EQ(emacs.makeInteger(42).asInteger(), 42);
        // REQUIRE_EQ(emacs.makeInteger(65536).asInteger(), 65536);

        // REQUIRE_EQ(emacs.makeInteger(-1), emacs.makeInteger(-1));
        // REQUIRE_NE(emacs.makeInteger(-1), emacs.makeInteger(-2));
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
