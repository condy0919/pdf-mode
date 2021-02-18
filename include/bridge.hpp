//! Thin bridge between Emacs and C++.
//!
//! SPDX-License-Identifier: GPL-2.0-or-later

#ifndef YAPDF_BRIDGE_HPP_
#define YAPDF_BRIDGE_HPP_

#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>

#include <emacs-module.h>

#include "emacs-module.h"
#include "expected.hpp"
#include "void.hpp"

#define YAPDF_EMACS_APPLY(env, f, ...) (env)->f((env), ##__VA_ARGS__)
#define YAPDF_EMACS_CHECK(env)                                                                                         \
    ({                                                                                                                 \
        emacs_value symbol, data;                                                                                      \
        const emacs_funcall_exit status = YAPDF_EMACS_APPLY(env, non_local_exit_check);                                \
        switch (status) {                                                                                              \
        case emacs_funcall_exit_return:                                                                                \
            break;                                                                                                     \
        case emacs_funcall_exit_signal:                                                                                \
            break;                                                                                                     \
        case emacs_funcall_exit_throw:                                                                                 \
            break;                                                                                                     \
        default:                                                                                                       \
            __builtin_unreachable();                                                                                   \
        }                                                                                                              \
        0                                                                                                              \
    })

namespace yapdf {
namespace emacs {
// forward
class Env;

/// A type that represents Lisp values.
///
/// Values of this type can be copied around, but are lifetime-bound to the `Env` they come from.
///
/// They are also "proxy values" that are only useful when converted to C++ native values, or used as arguments when
/// calling back into the Lisp runtime.
class Value {
public:
    Value(emacs_value val, Env& env) noexcept : val_(val), env_(env) {}

    /// Return the native handle of `emacs_value`
    emacs_value native() const noexcept {
        return val_;
    }

    /// Check whether the Lisp object is not `nil`.
    ///
    /// There can be multiple different values that represent `nil`.
    ///
    /// # Example
    ///
    /// ``` cpp
    ///
    /// ```
    ///
    /// # Note
    ///
    /// You could implement an equivalent test by using `intern` to get an `emacs_value` represent `nil`, then use ‘eq’,
    /// described above, to test for equality. But using this function is more convenient.
    operator bool() const noexcept;

    /// \{
    ///
    /// Check whether `*this` and `rhs` represent the same Lisp object.
    ///
    /// `operator==` corresponds to the Lisp `eq` function. For other kinds of equality comparisons, such as `eql`,
    /// `equal`, use `intern` and `funcall` to call the corresponding Lisp function.
    ///
    /// # Note
    ///
    /// Two `Value` objects that are different in the C sense might still represent the same Lisp object, so you must
    /// always call `operator==` to check for equality.
    bool operator==(const Value& rhs) const noexcept;

    bool operator!=(const Value& rhs) const noexcept {
        return !(*this == rhs);
    }
    /// \}

private:
    emacs_value val_;
    Env& env_;
};

/// Possible Emacs function call outcomes
enum class FuncallExit {
    /// Function has returned normally
    Return = 0,
    /// Function has signaled an error using `signal`
    Signal = 1,
    /// Function has exit using `throw`
    Throw = 2,
};

/// # Exception
///
/// Most languages have built-in support for exceptions and exception handling. Exception handling is commonly not
/// resumable in those languages, and when an exception is thrown, the program searches back through the stack of
/// function calls until an exception handler is found.
///
/// Some languages call for unwinding the stack as this search progresses. That is, if function `f`, containing a
/// handler `H` for exception `E`, calls function `g`, which in turn calls function `h`, and an exception `E` occurs in
/// `h`, then functions `h` and `g` may be terminated, and `H` in `f` will handle `E`.
///
/// If it's described in C++:
///
/// ``` cpp
/// void h() {
///     // ...
///     throw E();
/// }
///
/// void g() {
///     h();
/// }
///
/// void f() {
///     try {
///         // ...
///     } catch (const E& e) {
///         // the content of H handler
///         g();
///     }
/// }
/// ```
///
/// Excluding minor syntactic differences, there are only a couple of exception handling styles in use. In the most
/// popular style, an exception is initiated by a special statement (`throw` or `raise`) with an exception object (e.g.
/// Java) or a value of a special extendable enumerated type (e.g. SML). The scope for exception handlers starts with a
/// marker clause (try or the language's block starter such as begin) and ends in the start of the first handler clause
/// (`catch`, `except`, `rescue`). Several handler clauses can follow, and each can specify which exception types it
/// handles and what name it uses for the exception object.
///
/// A few languages also permit a clause (`else`) that is used in case no exception occurred before the end of the
/// handler's scope was reached.
///
/// More common is a related clause (`finally` or `ensure`) that is executed whether an exception occurred or not,
/// typically to release resources acquired within the body of the exception-handling block. Notably, C++ does not
/// provide this construct, since it encourages the Resource Acquisition Is Initialization (RAII) technique which frees
/// resources using destructors.
///
/// In its whole, exception handling code might look like this (in C++-like pseudocode):
///
/// ``` cpp
/// try {
///     std::string line = readline();
/// } catch (const std::exception& e) {
///     std::cerr << e.what() << std::endl;
/// }
/// ```
///
/// C supports various means of error checking, but generally is not considered to support "exception handling",
/// although the `setjmp` and `longjmp` standard library functions can be used to implement exception semantics.
///
/// # Exception handling implementation
///
/// The implementation of exception handling in programming languages typically involves a fair amount of support from
/// both a code generator and the runtime system accompanying a compiler. Two schemes are most common. The first,
/// dynamic registration, generates code that continually updates structures about the program state in terms of
/// exception handling. Typically, this adds a new element to the stack frame layout that knows what handlers are
/// available for the function or method associated with that frame; if an exception is thrown, a pointer in the layout
/// directs the runtime to the appropriate handler code. This approach is compact in terms of space, but adds execution
/// overhead on frame entry and exit. It was commonly used in many Ada implementations, for example, where complex
/// generation and runtime support was already needed for many other language features. Dynamic registration, being
/// fairly straightforward to define, is amenable to proof of correctness.
///
/// The second scheme, and the one implemented in many production-quality C++ compilers, is a table-driven approach.
/// This creates static tables at compile time and link time that relate ranges of the program counter to the program
/// state with respect to exception handling. Then, if an exception is thrown, the runtime system looks up the current
/// instruction location in the tables and determines what handlers are in play and what needs to be done. This approach
/// minimizes executive overhead for the case where an exception is not thrown. This happens at the cost of some space,
/// but this space can be allocated into read-only, special-purpose data sections that are not loaded or relocated until
/// an exception is actually thrown. This second approach is also superior in terms of achieving thread safety.
///
/// # Exception in Elisp
///
/// If a module or environment function wishes to signal an error, it sets the pending error state using
/// `non_local_exit_signal` or `non_local_exit_throw`; you can access the pending error state using
/// `non_local_exit_check` and `non_local_exit_get`.
///
/// If a nonlocal exit is pending, calling any environment function other than the functions used to manage nonlocal
/// exits immediately returns an unspecified value without further processing. You can make use of this fact to
/// occasionally skip explicit nonlocal exit checks.
///
/// | Enum                        | Desc                      |
/// |-----------------------------|---------------------------|
/// | `emacs_funcall_exit_return` | normal exit               |
/// | `emacs_funcall_exit_signal` | signal raised             |
/// | `emacs_funcall_exit_throw`  | jump to `catch` construct |
///
/// | Function                | Desc                              |
/// |-------------------------|-----------------------------------|
/// | `non_local_exit_check`  | Check whether an error is pending |
/// | `non_local_exit_get`    | Retrieve additional data          |
/// | `non_local_exit_signal` | Raise an signal                   |
/// | `non_local_exit_throw`  | Goto over functions               |
/// | `non_local_exit_clear`  | Reset the state of pending error  |
class Error {
public:
    Error(Value sym, Value data) noexcept : sym_(sym), data_(data) {}

    /// \{
    ///
    /// The Lisp `signal()` function signals an error named by `ERROR-SYMBOL`. The argument `DATA` is a list of
    /// additional Lisp objects relevant to the circumstances of the error.
    ///
    /// The argument `ERROR-SYMBOL` must be an "error symbol" - a symbol defined with `define-error`. This is how Emacs
    /// Lisp classifies different sorts of errors.
    ///
    /// If the error is not handled, the two arguments are used in printing the error message. Normally, this error
    /// message is provided by the `error-message` property of `ERROR-SYMBOL`. If `DATA` is non-`nil`, this is followed
    /// by a colon and a comma separated list of the unevaluated elements of `DATA`. For `'error`, the error message is
    /// the `CAR` of `DATA` (that must be a string). Subcategories of `file-error` are handled specially.
    ///
    /// The number and significance of the objects in `DATA` depends on `ERROR-SYMBOL`. For example, with a
    /// `wrong-type-argument` error, there should be two objects in the list: a predicate that describes the type that
    /// was expected, and the object that failed to fit that type.
    ///
    /// Both `ERROR-SYMBOL` and `DATA` are available to any error handlers that handle the error: `condition-case` binds
    /// a local variable to a list of the form `(ERROR-SYMBOL . DATA)`.
    ///
    /// The function `signal` never returns.
    ///
    /// ``` emacs-lisp
    /// (signal 'wrong-number-of-arguments '(x y))
    /// ;; error→ Wrong number of arguments: x, y
    ///
    /// (signal 'no-such-error '("My unknown error condition"))
    /// ;; error→ peculiar error: "My unknown error condition"
    /// ```
    ///
    /// - The `symbol()` is used to retrieve the `ERROR-SYMBOL` field of an error
    /// - The `data()` is used to retrieve the `DATA` field of an error
    Value symbol() const noexcept {
        return sym_;
    }

    Value data() const noexcept {
        return data_;
    }
    /// \}

    /// \{
    ///
    /// The purpose of `throw()` is to return from a return point previously established with `catch()`. The argument
    /// `tag` is used to choose among the various existing return points; it must be `eq` to the value specified in the
    /// `catch()`. If multiple return points match `tag`, the innermost one is used.
    ///
    /// The argument `value` is used as the value to return from that catch.
    ///
    /// If no return point is in effect with `tag`, then a `no-catch` error is signaled with data (tag value).
    ///
    /// `catch()` establishes a return point for the `throw()` function. The return point is distinguished from other
    /// such return points by `tag`, which may be any Lisp object except `nil`. The argument `tag` is evaluated normally
    /// before the return point is established.
    ///
    /// With the return point in effect, `catch` evaluates the forms of the body in textual order. If the forms execute
    /// normally (without error or nonlocal exit) the value of the last body form is returned from the `catch()`.
    ///
    /// If a `throw()` is executed during the execution of body, specifying the same value `tag`, the catch form exits
    /// immediately; the value it returns is whatever was specified as the second argument of `throw()`.
    ///
    /// # Examples
    ///
    /// ``` emacs-lisp
    /// (catch 'foo
    ///   (let ((x 1))
    ///     x))
    /// ;; 1
    ///
    /// (catch 'foo
    ///   (let ((x 1))
    ///     (throw 'foo x)))
    /// ;; 1
    ///
    /// (catch 'foo
    ///   (let ((x 1))
    ///     (throw 'bar x)))
    /// ;; error: (no-catch bar 1)
    /// ```
    ///
    /// - The `tag()` is used to retrieve the `TAG` field of an error
    /// - The `value()` is used to retrieve the `VALUE` field of an error
    Value tag() const noexcept {
        return sym_;
    }

    Value value() const noexcept {
        return data_;
    }
    /// \}

private:
    Value sym_;
    Value data_;
};

/// Possible return values for ProcessInput
enum class ProcessInputResult {
    /// Module code may continue
    Continue = 0,
    /// Module code should return control to Emacs as soon as possible
    Quit = 1,
};

/// Main point of interaction with the Lisp runtime
class Env {
public:
    explicit Env(emacs_env* env) noexcept : env_(env) {}

    /// Return the native handle of `emacs_env`
    emacs_env* native() const noexcept {
        return env_;
    }

    /// Provide feature to Emacs
    void provide(const char* feature) noexcept {
        emacs_value feat = YAPDF_EMACS_APPLY(env_, intern, feature);
        emacs_value prv = YAPDF_EMACS_APPLY(env_, intern, "provide");
        YAPDF_EMACS_APPLY(env_, funcall, prv, 1, &feat);
    }

    /// Return the canonical symbol whose name is `s`.
    // Value intern(const char* s) {
    //     const emacs_value val = YAPDF_EMACS_APPLY(env_, intern, s);
    //     return Value(this, val);
    // }

    /// Obtain the last function exit type for an environment.
    ///
    /// It never fails and always returns normally.
    ///
    /// - If there is no nonlocal exit pending, it returns `FuncallExit::Return`.
    /// - If there is a nonlocal raised by the Lisp `signal()` function, it returns `FuncallExit::Signal`
    /// - Otherwise, it returns `FuncallExit::Throw` which is raised by the Lisp `throw()` function
    FuncallExit checkError() noexcept {
        const emacs_funcall_exit status = YAPDF_EMACS_APPLY(env_, non_local_exit_check);
        return static_cast<FuncallExit>(status);
    }

    /// Retrieve additional data for nonlocal exits.
    ///
    /// - It shouldn't be called if there is no nonlocal exit pending, or use the `Error` value returned by this
    /// function is undefined.
    /// - If it's caused by `signal()`, the `Error` value holds the error symbol and relevant data
    /// - If it's caused by `throw()`, the `Error` value holds the tag and the catch value
    Error getError() noexcept {
        emacs_value sym = nullptr, data = nullptr;
        YAPDF_EMACS_APPLY(env_, non_local_exit_get, &sym, &data);
        return Error(Value(sym, *this), Value(data, *this));
    }

    /// Reset the pending-error state of `Env`.
    ///
    /// After calling `clearError()`, `checkError()` will again return `FuncallExit::Return`, and Emacs won't signal an
    /// error after returning from the current module function or module initialization function.
    ///
    /// You can use `clearError()` to ignore certain kinds of errors.
    void clearError() noexcept {
        YAPDF_EMACS_APPLY(env_, non_local_exit_clear);
    }

    /// `throwError()` is the module equivalent of the Lisp `throw()` function: it causes Emacs to perform a nonlocal
    /// jump to a catch block tagged with `tag`; the catch value will be `val`.
    ///
    /// `throwError()`, like all other environment functions, actually returns normally when seen as a C function.
    /// Rather, it causes Emacs to throw to the catch block once you return from the current module function or module
    /// initialization function. Therefore you should typically return quickly after requesting a jump with this
    /// function. If there was already a nonlocal exit pending when calling `throwError()`, the function does nothing;
    /// i.e. it doesn't overwrite catch tag and value. To do that, you must explicitly call `clearError()` first.
    void throwError(Error err) noexcept {
        YAPDF_EMACS_APPLY(env_, non_local_exit_throw, err.tag().native(), err.value().native());
    }

    /// `signalError()` is the module equivalent of the Lisp `signal()` function: it causes Emacs to signal an error of
    /// type `sym` with error data `data`. `data` should be a list.
    ///
    /// `signalError()`, like all other environment functions, actually returns normally when seen as a C function.
    /// Rather, it causes Emacs to signal an error once you return from the current module function or module
    /// initialization function. Therefore you should typically return quickly after signaling an error with this
    /// function. If there was already a nonlocal exit pending when calling `signalError()`, the function does nothing;
    /// i.e. it doesn't overwrite the error symbol and data. To do that, you must explicitly call `clearError()` first.
    void signalError(Error err) noexcept {
        YAPDF_EMACS_APPLY(env_, non_local_exit_signal, err.symbol().native(), err.data().native());
    }

private:
    emacs_env* env_;
};

} // namespace emacs

/// An Emacs instance
class Emacs {
    friend class Value;

public:
    /// An Emacs Lisp value
    class Value {
        friend class Emacs;

    public:
        // TODO grow me!
        enum class Type {
            Integer,
            Symbol,
            String,
            Cons,
            Float,
            Unknown,
        };

        /// Return the type of a Lisp symbol. It corresponds exactly to the Lisp `type-of` function.
        [[nodiscard]] Type typeOf() const {
            // const Value type(emacs_, YAPDF_EMACS_APPLY(emacs_->env_, type_of, value_));
            // if (type == emacs_->intern("integer")) {
            //     return Type::Integer;
            // } else if (type == emacs_->intern("symbol")) {
            //     return Type::Symbol;
            // } else if (type == emacs_->intern("string")) {
            //     return Type::String;
            // } else if (type == emacs_->intern("cons")) {
            //     return Type::Cons;
            // } else if (type == emacs_->intern("float")) {
            //     return Type::Float;
            // }
            return Type::Unknown;
        }

        /// Return the integral value stored in the Emacs integer object.
        ///
        /// If it doesn't represent an integer object, Emacs will signal an error of type
        /// `wrong-type-argument`. If the integer represented by `Value` can't be represented as
        /// `std::intmax_t`, Emacs will signal an error of type `overflow-error`.
        [[nodiscard]] std::intmax_t asInteger() const {
            return YAPDF_EMACS_APPLY(emacs_->env_, extract_integer, value_);
        }

        /// Return the value stored in the Emacs floating-point number.
        ///
        /// If it doesn't represent a floating-point object, Emacs will signal an error of type
        /// `wrong-type-argument`.
        [[nodiscard]] double asFloat() const {
            return YAPDF_EMACS_APPLY(emacs_->env_, extract_float, value_);
        }

#if EMACS_MAJOR_VERSION >= 27
        //   bool (*extract_big_integer) (emacs_env *env, emacs_value arg, int
        //   *sign,
        //                                ptrdiff_t *count, emacs_limb_t
        //                                *magnitude)
        //     EMACS_ATTRIBUTE_NONNULL (1);

        /// Return the value stored in the Emacs timestamp with nanoseconds precision.
        ///
        /// If you need to deal with time values that not representable by `struct timespec`, or if
        /// you want higher precision, call the Lisp function `encode-time` and work with its return
        /// value.
        ///
        /// \since Emacs 27
        [[nodiscard]] std::chrono::nanoseconds asTime() const {
            const struct timespec ts = YAPDF_EMACS_APPLY(emacs_->env_, extract_time, value_);
            return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(ts.tv_sec) +
                                                                        std::chrono::nanoseconds(ts.tv_nsec));
        }

#endif

        void copy_string_contents();

        void make_user_ptr();
        void get_user_ptr();
        void set_user_ptr();

        void get_user_finalizer();
        void set_user_finalizer();

        void vec_get();
        void vec_set();
        void vec_size();

        /// Get the native implementation `emacs_value`
        [[nodiscard]] emacs_value native() const {
            return value_;
        }

    private:
        Value(Emacs* emacs, emacs_value value) : emacs_(emacs), value_(value) {}

        Emacs* emacs_;
        emacs_value value_;
    };

    Emacs(emacs_env* env) : env_(env) {}

    // memory management
    Value makeGlobalRef(Value value);
    void freeGlobalRef(Value value);

    // non-local exit handling
    void non_local_exit_check();
    void non_local_exit_clear();
    void non_local_exit_get();
    void non_local_exit_signal();
    void non_local_exit_throw();

    /// Create an Emacs integer object from a C integer value.
    ///
    /// If the value can't be represented as an Emacs integer, Emacs will signal an error of type
    /// `overflow-error`.
    Value makeInteger(std::intmax_t v) {
        return Value(this, YAPDF_EMACS_APPLY(env_, make_integer, v));
    }

    /// Create an Emacs floating-point number from a C floating-point value.
    Value makeFloat(double v) {
        return Value(this, YAPDF_EMACS_APPLY(env_, make_float, v));
    }

    /// Create a multibyte Lisp string object.
    ///
    /// If `len` is larger than the maximum allowed Emacs string length, Emacs will raise an
    /// `overflow-error` signal. Otherwise, Emacs treats the memory at `s` as the UTF-8
    /// representation of a string.
    ///
    /// If the memory block delimited by `s` and `len` contains a valid UTF-8 string, the result
    /// value will be a multibyte Lisp string that contains the same sequence of Unicode scalar
    /// values as represented by `s`. Otherwise, the return value will be a multibyte Lisp string
    /// with unspecified contents; in practice, Emacs will attempt to detect as many valid UTF-8
    /// subsequence in `s` as possible and treat the rest as undecodable bytes, but you shouldn't
    /// rely on any specific behavior in this case.
    ///
    /// The returned Lisp string will not contain any text properties. To create a string containing
    /// text properties, use `funcall` to call functions such as `propertize`.
    ///
    /// `makeString` can't create strings that contains characters that are not valid Unicode scalar
    /// values. Such strings are rare, but occur from time to time; examples are strings with UTF-16
    /// surrogate code points or strings with extended Emacs characters that don't correspond to
    /// Unicode code points. To create such a Lisp string, call e.g. the function `string` and pass
    /// the desired character values as integers.
    ///
    /// # Note
    ///
    /// `s` must be null-terminated.
    Value makeString(const char* s, std::size_t len) {
        return Value(this, YAPDF_EMACS_APPLY(env_, make_string, s, len));
    }

    Value makeString(const char* s) {
        return makeString(s, std::strlen(s));
    }

    Value makeString(const std::string& s) {
        return makeString(s.c_str(), s.size());
    }

    // TODO
    Value makeFunction() {
        return Value(this, YAPDF_EMACS_APPLY(env_, make_function, 0, 0, nullptr, "docstring", nullptr));
    }

    // function register
    void funcall();

#if EMACS_MAJOR_VERSION >= 26
    bool should_quit();
#endif

#if EMACS_MAJOR_VERSION >= 27
    // enum emacs_process_input_result (*process_input) (emacs_env *env)
    //     EMACS_ATTRIBUTE_NONNULL (1);

    //   struct timespec (*extract_time) (emacs_env *env, emacs_value arg)
    //     EMACS_ATTRIBUTE_NONNULL (1);

    /// Create a
    ///
    /// \since Emacs 27
    Value makeTime(std::chrono::seconds secs, std::chrono::nanoseconds ns) {
        const struct timespec ts = {
            .tv_sec = secs.count(),
            .tv_nsec = ns.count(),
        };
        return Value(this, YAPDF_EMACS_APPLY(env_, make_time, ts));
    }

    Value makeTime(std::chrono::nanoseconds ns) {
        const auto secs = std::chrono::duration_cast<std::chrono::seconds>(ns);
        ns -= secs;
        return makeTime(secs, ns);
    }

//   emacs_value (*make_big_integer) (emacs_env *env, int sign, ptrdiff_t count,
//                                    const emacs_limb_t *magnitude)
//     EMACS_ATTRIBUTE_NONNULL (1);
#endif

#if EMACS_MAJOR_VERSION >= 28
    // void (*(*EMACS_ATTRIBUTE_NONNULL (1)
    //             get_function_finalizer) (emacs_env *env,
    //                                      emacs_value arg)) (void *)
    //                                      EMACS_NOEXCEPT;

    //   void (*set_function_finalizer) (emacs_env *env, emacs_value arg,
    //                                   void (*fin) (void *) EMACS_NOEXCEPT)
    //     EMACS_ATTRIBUTE_NONNULL (1);

    //   int (*open_channel) (emacs_env *env, emacs_value pipe_process)
    //     EMACS_ATTRIBUTE_NONNULL (1);

    void makeInteractive(Value f, const char* spec) {}

    //   void (*make_interactive) (emacs_env *env, emacs_value function,
    //                             emacs_value spec)
    //     EMACS_ATTRIBUTE_NONNULL (1);

    //   /* Create a unibyte Lisp string from a string.  */
    //   emacs_value (*make_unibyte_string) (emacs_env *env,
    // 				      const char *str, ptrdiff_t len)
    //     EMACS_ATTRIBUTE_NONNULL(1, 2);
#endif

private:
    emacs_env* env_;
};
} // namespace yapdf

#endif // YAPDF_BRIDGE_HPP_
