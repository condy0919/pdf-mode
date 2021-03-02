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

        // a nonnull-terminated string
        const char s[3] = {'a', 'b', 'c'};
        auto nt = e.make<Value::Type::String>(s, 3).value();
        REQUIRE_EQ(nt.as<Value::Type::String>().value(), "abc");

        // an empty string
        auto empty = e.make<Value::Type::String>("").value();
        REQUIRE_EQ(empty.as<Value::Type::String>().value(), "");

        // 你好 in gb2312, it should fail
        const std::uint8_t hello[] = {0xe3, 0xc4, 0xc3, 0xba};
        auto result = e.make<Value::Type::String>((const char*)hello, 4);

#if EMACS_MAJOR_VERSION >= 28
        // seems like utf-8 check when `make_string` implemented only on Emacs 28
        REQUIRE(result.hasError());
        REQUIRE_EQ(result.error().status(), yapdf::emacs::FuncallExit::Signal);
#endif
    }

    SUBCASE("Vector") {
        // (vector 1 "foo" 1.2) -> [1 "foo" 1.2]
        auto val = e.call("vector", 1, "foo", 1.2).value();

        // (type-of vec) -> 'vector
        REQUIRE_EQ(val.typeOf(), e.intern("vector").value());

        // (length [1 "foo" 1.2]) -> 3
        REQUIRE_EQ(val.size(), 3);

        // vec[0] == 1
        REQUIRE_EQ(Value(val[0]).as<Value::Type::Int>().value(), 1);

        // vec[1] == "foo"
        REQUIRE_EQ(Value(val[1]).as<Value::Type::String>().value(), "foo");

        // vec[2] == 1.2
        REQUIRE_EQ(Value(val[2]).as<Value::Type::Float>().value(), doctest::Approx(1.2));

        // (eq vec[1] "foo") -> false
        REQUIRE_NE(Value(val[1]), e.make<Value::Type::String>("foo").value());

        // (equal vec[1] "foo") -> true
        REQUIRE(e.call("equal", Value(val[1]), "foo").value());

        // (equal vec[1] "bar") -> false
        REQUIRE_FALSE(e.call("equal", Value(val[1]), "bar").value());

        // (aset vec 1 "bar")
        val[1] = e.make<Value::Type::String>("bar").value();

        // Now (equal vec[1] "bar") -> true
        REQUIRE(e.call("equal", Value(val[1]), "bar").value());
    }

    SUBCASE("UserPtr") {
        // c++ -> lisp
        auto val =
            e.make<Value::Type::UserPtr>(nullptr, [](void* p) EMACS_NOEXCEPT { REQUIRE_EQ(p, nullptr); }).value();

        // (type-of val) -> 'user-ptr
        REQUIRE_EQ(val.typeOf(), e.intern("user-ptr").value());

        // lisp -> c++
        REQUIRE_EQ(val.as<Value::Type::UserPtr>().value(), nullptr);

        const auto fin = val.finalizer();
        fin(nullptr);

        // discard the custom finalizer
        val.finalizer(nullptr);
        val.reset((void*)1);

        REQUIRE_EQ(val.as<Value::Type::UserPtr>().value(), (void*)1);
    }

#if EMACS_MAJOR_VERSION >= 27
    SUBCASE("Time") {
        // c++ -> lisp
        auto val = e.make<Value::Type::Time>(std::chrono::seconds(1), std::chrono::nanoseconds(50)).value();

        // (type-of '(TICKS . HZ)) -> 'cons
        REQUIRE_EQ(val.typeOf(), e.intern("cons").value());

        // lisp -> c++
        REQUIRE_EQ(val.as<Value::Type::Time>().value(), std::chrono::seconds(1) + std::chrono::nanoseconds(50));
    }
#endif

#if EMACS_MAJOR_VERSION >= 28
    SUBCASE("ByteString") {
        // c++ -> lisp
        auto val = e.make<Value::Type::ByteString>("foobarbaz").value();

        // (type-of "foobarbaz") -> 'string
        REQUIRE_EQ(val.typeOf(), e.intern("string").value());

        // lisp -> c++
        REQUIRE_EQ(val.as<Value::Type::String>().value(), "foobarbaz");

        // a string with two '\0's
        auto zeros = e.make<Value::Type::ByteString>("\0\0", 2).value();
        REQUIRE_EQ(zeros.as<Value::Type::String>().value(), std::string("\0\0", 2));

        // a nonnull-terminated string
        const char s[3] = {'a', 'b', 'c'};
        auto nt = e.make<Value::Type::ByteString>(s, 3).value();
        REQUIRE_EQ(nt.as<Value::Type::String>().value(), "abc");

        // an empty string
        auto empty = e.make<Value::Type::ByteString>("").value();
        REQUIRE_EQ(empty.as<Value::Type::String>().value(), "");

        // 你好 in gb2312
        const std::uint8_t hello[] = {0xe3, 0xc4, 0xc3, 0xba};
        auto hi = e.make<Value::Type::ByteString>((const char*)hello, 4).value();
        REQUIRE_EQ(hi.as<Value::Type::String>().value(), std::string("\xe3\xc4\xc3\xba", 4));
    }
#endif
}

TEST_CASE("Function") {

}

TEST_CASE("Funcall") {
    yapdf::emacs::Env e(env);
    using yapdf::emacs::Value;

    auto val = e.call("length", "abc");
    REQUIRE_EQ(val.value().as<Value::Type::Int>().value(), 3);

    // zero args
    auto time = e.call("current-time");
    REQUIRE(time.hasValue());
}

TEST_CASE("Signal") {
    yapdf::emacs::Env e(env);
    using yapdf::emacs::Value;

}

TEST_CASE("Throw") {

}

#if EMACS_MAJOR_VERSION >= 28
TEST_CASE("openChannel") {
    yapdf::emacs::Env e(env);
    using yapdf::emacs::Value;

    // create a new buffer
    Value buffer = e.call("generate-new-buffer", " *temp* ").expect("generate-new-buffer");

    // make a pipe process
    Value proc = e.call("make-pipe-process", e.intern(":name"), "test", // the process name
                        e.intern(":buffer"), buffer,                    // buffer-name is also acceptable
                        e.intern(":noquery"), e.intern("t"))            // no query when exiting Emacs
                     .expect("make-pipe-process");

    // duplicate the fd
    int fd = e.openChannel(proc).expect("open-channel");

    // send msg from C++
    const char msg[] = "Hi from C++";
    const int ret = write(fd, msg, std::strlen(msg));
    REQUIRE_EQ(ret, std::strlen(msg));
    REQUIRE_EQ(close(fd), 0);

    // make process output ready for Emacs
    Value result = e.call("accept-process-output", proc).expect("accept-process-output");
    REQUIRE(static_cast<bool>(result));

    // switch buffer
    e.call("set-buffer", buffer).expect("set-buffer");

    // get the contents of the current string
    const std::string got = e.call("buffer-string").expect("buffer-string").as<Value::Type::String>().value();
    REQUIRE_EQ(got, msg);

    e.call("kill-buffer", buffer).expect("kill-buffer");
}
#endif
