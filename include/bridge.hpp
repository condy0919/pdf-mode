//! Thin bridge between Emacs and C++.
//!
//! SPDX-License-Identifier: GPL-2.0-or-later

#ifndef YAPDF_BRIDGE_HPP_
#define YAPDF_BRIDGE_HPP_

#include <cassert>
#include <chrono>
#include <climits>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <emacs-module.h>

#include "expected.hpp"
#include "likely.hpp"
#include "overload.hpp"
#include "requires.hpp"
#include "unreachable.hpp"
#include "void.hpp"

#define YAPDF_EMACS_APPLY(env, f, ...) ((env).native())->f((env).native(), ##__VA_ARGS__)
#define YAPDF_EMACS_APPLY_CHECK(env, f, ...)                                                                           \
    ({                                                                                                                 \
        const auto ret = ::yapdf::internal::return_((YAPDF_EMACS_APPLY((env), f, ##__VA_ARGS__), ::yapdf::Void{}));    \
        const auto status = (env).checkError();                                                                        \
        switch (status) {                                                                                              \
        case ::yapdf::emacs::FuncallExit::Return:                                                                      \
            break;                                                                                                     \
        default:                                                                                                       \
            return ::yapdf::Unexpected(({                                                                              \
                auto err = (env).getError();                                                                           \
                (env).clearError();                                                                                    \
                err;                                                                                                   \
            }));                                                                                                       \
        }                                                                                                              \
        ret;                                                                                                           \
    })

// Check that __COUNTER__ is defined and that __COUNTER__ increases by 1 every time it is expanded.
//
// X + 1 == X + 0 is used in case X is defined to be empty. If X is empty the expression becomes (+1 == +0).
#if defined(__COUNTER__) && (__COUNTER__ + 1 == __COUNTER__ + 0)
#define YAPDF_EMACS_PRIVATE_UNIQUE_ID __COUNTER__
#else
#define YAPDF_EMACS_PRIVATE_UNIQUE_ID __LINE__
#endif

#define YAPDF_EMACS_PRIVATE_NAME(n) YAPDF_EMACS_PRIVATE_CONCAT(_yapdf_emacs_, YAPDF_EMACS_PRIVATE_UNIQUE_ID, n)
#define YAPDF_EMACS_PRIVATE_CONCAT(a, b, c) YAPDF_EMACS_PRIVATE_CONCAT2(a, b, c)
#define YAPDF_EMACS_PRIVATE_CONCAT2(a, b, c) a##b##c
#define YAPDF_EMACS_PRIVATE_DECLARE(n) static ::yapdf::emacs::Defun* YAPDF_EMACS_PRIVATE_NAME(n) [[maybe_unused]]

/// `YAPDF_EMACS_DEFUN` is used to provide elisp function register.
///
/// Currently three DEFUN forms are allowed.
///
/// ``` cpp
/// // form 1
/// static void foo1(Env&, void* p) {
///     const char* s = (const char*)p;
///     std::printf("%s\n", s);
/// }
/// YAPDF_EMACS_DEFUN(foo1, "foo1", "The foo1 function defined at C++.");
///
/// // form 2
/// static Expected<Value, Error> foo2(Env& e, Value args[], std::size_t n) {
///     return e.intern("nil");
/// }
/// YAPDF_EMACS_DEFUN(foo2, 1, 1, "foo2", "foo2 does nothing.");
///
/// // form 3
/// static emacs_value foo3(emacs_env* env, std::ptrdiff_t, emacs_value args[], void*) EMACS_NOEXCEPT {
///     emacs_value arg = args[1];
///
///     return env->is_not_nil(env, arg) ? env->intern(env, "t") : env->intern(env, "nil");
/// }
/// YAPDF_EMACS_DEFUN(foo3, 1, 1, "foo3", "foo3 checks whether  the second value is nil");
/// ```
///
/// \see defsubr
/// \see DefunRawFunction
/// \see DefunWrappedFunction
/// \see DefunUniversalFunction
#define YAPDF_EMACS_DEFUN(...)                                                                                         \
    YAPDF_EMACS_PRIVATE_DECLARE(n) = ::yapdf::emacs::DefunRegistry::registra(::yapdf::emacs::defsubr(__VA_ARGS__));

namespace {
// Those values/types are defined at emacs/src/lisp.h

// Number of bits in a Lisp_Object tag
constexpr auto GCTYPEBITS = 3;

// Signed integer type that is wide enough to hold an Emacs value
using EmacsIntType =
#if INTPTR_MAX <= INT_MAX
    int
#elif INTPTR_MAX <= LONG_MAX
    long int
#elif INTPTR_MAX <= LLONG_MAX
    long long int
#else
#error "INTPTR_MAX too large"
#endif
    ;

// The maximum value that can be stored in a `EmacsIntType`, assuming all bits other than the type bits contribute to a
// nonnegative signed value.
constexpr auto VAL_MAX = std::numeric_limits<EmacsIntType>::max() >> (GCTYPEBITS - 1);

// Whether the least-significant bits of an `EmacsIntType` contain the tag.
//
// On hosts where pointers-as-ints do not exceed `VAL_MAX` / 2, `USE_LSB_TAG` is:
// 1. unnecessary, because the top bits of an `EmacsIntType` are unused
// 2. slower, because it typically requires extra masking
//
// So, `USE_LSB_TAG` is true only on hosts where it might be useful.
constexpr bool USE_LSB_TAG = (VAL_MAX / 2 < INTPTR_MAX);

// Mask for the value (as opposed to the type bits) of a Lisp object
constexpr auto VALMASK = USE_LSB_TAG ? -(1 << GCTYPEBITS) : VAL_MAX;

// Number of bits in a Lisp_Object value, not counting the tag
constexpr auto VALBITS = sizeof(EmacsIntType) * CHAR_BIT - GCTYPEBITS;
} // namespace

namespace yapdf {
namespace internal {
template <typename T>
inline T&& return_(T&& x) noexcept {
    return std::forward<T>(x);
}
} // namespace internal

namespace emacs {
// forward
class Env;
class Value;
class Error;
enum class FuncallExit;

/// Function prototype for the module lisp functions. These must not throw C++ exceptions.
///
/// We manually typedef `EmacsFunction` for compatibilities.
using EmacsFunction = emacs_value (*)(emacs_env*, std::ptrdiff_t, emacs_value*, void*) EMACS_NOEXCEPT;

// HACK: Trick to allow `YAPDF_EMACS_APPLY_CHECK` to handle Emacs module functions that return void
template <typename T>
inline T&& operator,(T&& x, Void) noexcept {
    return std::forward<T>(x);
}

/// \defgroup Elisp Register
/// \{
///
/// The base class for defining elisp functions.
///
/// \see DefunRawFunction
/// \see DefunWrappedFunction
/// \see DefunUniversalFunction
class Defun {
public:
    virtual void def(Env&) noexcept = 0;

protected:
    Defun(const char* name, const char* docstring) noexcept : name_(name), docstring_(docstring) {}

    const char* name_;
    const char* docstring_;
};

/// Class for defining elisp functions in raw form.
///
/// ``` cpp
/// emacs_value (*)(emacs_env*, std::ptrdiff_t, emacs_value*, void*) EMACS_NOEXCEPT;
/// ```
///
/// When using `DefunRawFunction`, the extra `void*` parameter is `nullptr`. Use `Env::make<Value::Type::Function>()`
/// directly if you want to customize the extra `void*` parameter.
class DefunRawFunction : public Defun {
public:
    DefunRawFunction(EmacsFunction f, std::ptrdiff_t min_arity, std::ptrdiff_t max_arity, const char* name,
                     const char* docstring) noexcept
        : Defun(name, docstring), min_arity_(min_arity), max_arity_(max_arity), f_(f) {}

    void def(Env&) noexcept override;

private:
    std::ptrdiff_t min_arity_;
    std::ptrdiff_t max_arity_;
    EmacsFunction f_;
};

/// Class for defining elisp functions in wrapped form.
///
/// ``` cpp
/// Expected<Value, Error> (*)(Env&, Value[], std::size_t);
/// ```
class DefunWrappedFunction : public Defun {
public:
    DefunWrappedFunction(Expected<Value, Error> (*f)(Env&, Value[], std::size_t), std::ptrdiff_t min_arity,
                         std::ptrdiff_t max_arity, const char* name, const char* docstring) noexcept
        : Defun(name, docstring), min_arity_(min_arity), max_arity_(max_arity), f_(f) {}

    void def(Env&) noexcept override;

private:
    std::ptrdiff_t min_arity_;
    std::ptrdiff_t max_arity_;
    Expected<Value, Error> (*f_)(Env&, Value[], std::size_t);
};

/// Class for defining elisp functions in universal form.
///
/// ``` cpp
/// R (*)(Env&, Args...);
/// ```
template <typename R, typename... Args>
class DefunUniversalFunction : public Defun {
public:
    DefunUniversalFunction(R (*f)(Env&, Args...), const char* name, const char* docstring) noexcept
        : Defun(name, docstring), f_(f) {}

    void def(Env&) noexcept override;

private:
    R (*f_)(Env&, Args...);
};

template <typename R, typename... Args>
DefunUniversalFunction(R (*)(Env&, Args...), const char*, const char*) -> DefunUniversalFunction<R, Args...>;

/// Class for managing registered elisp functions.
class DefunRegistry {
public:
    static DefunRegistry& getInstance() noexcept;

    static Defun* registra(Defun* defun) noexcept;

    void add(std::unique_ptr<Defun> defun);

    void clear() noexcept;

    void def(Env&) noexcept;

private:
    DefunRegistry() noexcept = default;

    std::vector<std::unique_ptr<Defun>> defuns_;
};

inline static Defun* defsubr(EmacsFunction f, std::ptrdiff_t min_arity, std::ptrdiff_t max_arity, const char* name,
                             const char* docstring) {
    return new DefunRawFunction(f, min_arity, max_arity, name, docstring);
}

inline static Defun* defsubr(Expected<Value, Error> (*f)(Env&, Value[], std::size_t), std::ptrdiff_t min_arity,
                             std::ptrdiff_t max_arity, const char* name, const char* docstring) {
    return new DefunWrappedFunction(f, min_arity, max_arity, name, docstring);
}

template <typename R, typename... Args>
inline static Defun* defsubr(R (*f)(Env&, Args...), const char* name, const char* docstring) {
    return new DefunUniversalFunction(f, name, docstring);
}
/// \}

/// Global reference
///
/// Most Emacs values have a short lifetime that ends once their owning `Env` goes out of scope. However, occasionally
/// it's useful to have values with a longer lifetime when creating some objects over and over again incurs a too high
/// CPU cost.
///
/// `GlobalRef` are normal `emacs_value` objects, with one key difference: they are not bound to the lifetime of any
/// environment. Rather, you can use them, once created, whenever any environment is active. Be aware that using global
/// reference, like all global state, incurs a readability cost on your code: with global references, you have to keep
/// track which parts of your code modify which reference. You are also responsible for managing the lifetime of global
/// references, whereas local values go out of scope manually.
class GlobalRef {
    friend class Value;

public:
    /// Construct `GlobalRef` in an uninitialized state.
    ///
    /// Use of such state `GlobalRef` is UB.
    GlobalRef() noexcept = default;

    /// Return the native handle of `emacs_value`
    [[nodiscard]] emacs_value native() const noexcept {
        return val_;
    }

    /// Free this global reference
    void free(Env& env) noexcept;

    /// Return the underlying `Value`, scoping its lifetime to the given `Env`
    Value bind(Env& env) noexcept;

private:
    /// Create `GlobalRef` from `emacs_value`.
    ///
    /// User should NOT call this function directly.
    explicit GlobalRef(emacs_value val) noexcept : val_(val) {}

    emacs_value val_;
};

/// A type that represents Lisp values.
///
/// Values of this type can be copied around, but are lifetime-bound to the `Env` they come from.
///
/// They are also "proxy values" that are only useful when converted to C++ native values, or used as arguments when
/// calling back into the Lisp runtime.
class Value {
public:
    /// A vector proxy type to `Value` to provide vector-like operations.
    ///
    /// # Example
    ///
    /// ``` cpp
    /// Value vec = env.call("vector", 101, 202, 303, 404).expect("vector");
    ///
    /// vec[10] = 1;
    /// const Value v = vec[11];
    /// ```
    class VectorProxy {
    public:
        VectorProxy(std::size_t idx, emacs_value val, Env& env) noexcept : idx_(idx), val_(val), env_(env) {}

        /// Set the index-th element of the vector to v.
        ///
        /// If index is not less than the number of elements in vector, Emacs will signal an error of type
        /// `args-out-of-range`.
        ///
        /// \see Value::operator[]
        VectorProxy& operator=(Value v) noexcept;

        /// Return the index-th element of the vector.
        ///
        /// If index is not less than the number of elements in vector, Emacs will signal an error of type
        /// `args-out-of-range`.
        ///
        /// \see Value::operator[]
        operator Value() const noexcept;

    private:
        std::size_t idx_;
        emacs_value val_;
        Env& env_;
    };

    /// Types that a Lisp value can be constructed from or cast to.
    enum class Type {
        Int,
        Float,
        String,
        Time,
        Function,
        ByteString,
        UserPtr,
    };

    /// Types that a Lisp value can be represented.
    ///
    /// `LispType` is used to distinguish the type of a `Value` rather than constructing/casting a `Value`.
    ///
    /// | Type         | Int Value                 |
    /// |--------------|---------------------------|
    /// | `Symbol`     | 0                         |
    /// | `Unused`     | 1                         |
    /// | `Int0`       | 2                         |
    /// | `Cons`       | 3 if `USE_LSB_TAG` else 6 |
    /// | `String`     | 4                         |
    /// | `VectorLike` | 5                         |
    /// | `Int1`       | 6 if `USE_LSB_TAG` else 3 |
    /// | `Float`      | 7                         |
    ///
    /// Where `Int0` represents an Integer in [-2^20, 2^20] on my machine, `Int1` has no such restrictions.
    ///
    /// \see type()
    enum class LispType {
        Symbol = 0,
        Unused = 1,
        Int0 = 2,
        Cons = USE_LSB_TAG ? 3 : 6,
        String = 4,
        VectorLike = 5,
        Int1 = USE_LSB_TAG ? 6 : 3,
        Float = 7,
    };

    /// Construct a new `Value` from Emacs native types
    Value(emacs_value val, Env& env) noexcept : val_(val), env_(env) {}

    /// Return the native handle of `emacs_value`
    [[nodiscard]] emacs_value native() const noexcept {
        return val_;
    }

    /// Return the type of a Lisp symbol. It corresponds exactly to the Lisp `type-of` function.
    [[nodiscard]] Value typeOf() const noexcept;

    /// Return the type of `Value` in numeric.
    ///
    /// **Unofficial API**
    ///
    /// It depends on the Emacs object internal implementation. Use at your own risk.
    [[nodiscard]] LispType type() const noexcept;

    /// Create a new `GlobalRef` for this Value.
    [[nodiscard]] GlobalRef ref() const noexcept;

    /// Return the name of symbol.
    ///
    /// \see the Lisp `symbol-name` function
    Expected<std::string, Error> name() const noexcept;

    /// Return the value of symbol.
    ///
    /// \see the Lisp `symbol-value` function
    Expected<Value, Error> value() const noexcept;

    /// \defgroup Vector
    /// \{
    ///
    /// Return the number of elements in the vector.
    ///
    /// Make sure that `Value` represents a Lisp vector, or Emacs will signal an error of type `wrong-type-argument` and
    /// use of the returned value is undefined.
    [[nodiscard]] std::size_t size() const noexcept;

    /// Make `Value` vector-like.
    ///
    /// Make sure that `Value` represents a Lisp vector, or Emacs will signal an error of type `wrong-type-argument`. If
    /// idx is not less than the number of elements in vector, Emacs will signal an error of type `args-out-of-range` in
    /// further operations.
    ///
    /// \see VectorProxy
    VectorProxy operator[](std::size_t idx) noexcept;

    /// Return the idx-th of `Value`.
    ///
    /// Unlike `std::vector`, `Vector::at` will return an `Expected<T, E>` type.
    Expected<Value, Error> at(std::size_t idx) noexcept;
    /// \}

    /// Convert from Emacs value to native C++ types.
    ///
    /// The relationship between the `type` and native C++ type can be described with the following table:
    ///
    /// | `type`          | C++ type                   |
    /// |-----------------|----------------------------|
    /// | `Type::Int`     | `std::intmax_t`            |
    /// | `Type::Float`   | `double`                   |
    /// | `Type::String`  | `std::string`              |
    /// | `Type::Time`    | `std::chrono::nanoseconds` |
    /// | `Type::UserPtr` | `void*`                    |
    ///
    /// # Int
    ///
    /// Return the integral value stored in the Emacs integer object.
    ///
    /// If it doesn't represent an integer object, Emacs will signal an error of type `wrong-type-argument`. If the
    /// integer represented by `Value` can't be represented as `std::intmax_t`, Emacs will signal an error of type
    /// `overflow-error`.
    ///
    /// # Float
    ///
    /// Return the value stored in the Emacs floating-point number.
    ///
    /// If it doesn't represent a floating-point object, Emacs will signal an error of type `wrong-type-argument`.
    ///
    /// # String
    ///
    /// Return the value stored in the Emacs string.
    ///
    /// If value doesn't represent a Lisp string, Emacs signals an error of type `wrong-type-argument`.
    ///
    /// Emacs copies the UTF-8 representation of the characters out. If value contains only Unicode scalar values (i.e.
    /// it's either a unibyte string containing only ASCII characters or a multibyte string containing only characters
    /// that are Unicode scalar values), the string stored in buffer will be a valid UTF-8 string representing the same
    /// sequence of scalar values as value. Otherwise, the contents of buffer are unspecified; in practice, Emacs
    /// attempts to convert scalar values to UTF-8 and leaves other bytes alone, but you shouldn't rely on any specific
    /// behavior in this case.
    ///
    /// There's no environment function to extract string properties. Use the usual Emacs functions such as
    /// `get-text-property` for that.
    ///
    /// To deal with strings that don't represent sequences of Unicode scalar values, you can use Emacs functions such
    /// as `length` and `aref` to extract the character values directly.
    ///
    /// # Time
    ///
    /// Return the value stored in the Emacs timestamp with nanoseconds precision.
    ///
    /// If you need to deal with time values that not representable by `struct timespec`, or if you want higher
    /// precision, call the Lisp function `encode-time` and work with its return value.
    ///
    /// Available since Emacs 27.
    ///
    /// # UserPtr
    ///
    /// Return the user pointer embedded in the user pointer object.
    ///
    /// If it doesn't represent a user pointer object, Emacs will signal an error of type `wrong-type-argument`.
    template <Type type>
    auto as() const noexcept {
        static_assert(type != Type::Function, "Can't convert lisp function to native C++ function");
        static_assert(type != Type::ByteString, "Use Type::String instead");
        return as(std::integral_constant<Type, type>{});
    }

    /// \defgroup UserPtr
    /// \{
    ///
    /// Reset the user pointer to p.
    ///
    /// Make sure that `Value` represents a user pointer object, or Emacs will signal an error of type
    /// `wrong-type-argument`.
    void reset(void* p) noexcept;

    /// Return the user pointer finalizer embedded in the user pointer object; this is the fin value that you have
    /// passed to `make_user_ptr`. If it doesn't have a custom finalizer, Emacs returns `nullptr`.
    ///
    /// Make sure that `Value` represents a user pointer object, or Emacs will signal an error of type
    /// `wrong-type-argument`.
    [[nodiscard]] auto finalizer() const noexcept -> void (*)(void*) EMACS_NOEXCEPT;

    /// Reset the user pointer finalizer with fin. fin can be `nullptr` if it doesn't need custom finalization.
    ///
    /// Make sure that `Value` represents a user pointer object, or Emacs will signal an error of type
    /// `wrong-type-argument`.
    void finalizer(void (*fin)(void*) EMACS_NOEXCEPT) noexcept;
    /// \}

#if EMACS_MAJOR_VERSION >= 28
    /// Make function interactive.
    ///
    /// By default, module functions created by `make_function` are not interactive. To make theme interactive, you can
    /// use `interactive` to make function interactive using the `interactive` specification. Note that there is no
    /// native module support for retrieving the interactive specification of a module function. Use the function
    /// `interactive-form` for that. And it is not possible to make a module function non-interactive once you have made
    /// it interactive using this function.
    ///
    /// \see the Lisp interactive function
    /// \since Emacs 28
    Expected<Void, Error> interactive(const char* spec) noexcept;

    /// Return the function finalizer associated with the module function returned by `make_function`. If no finalizer
    /// is associated with the function, `nullptr` is returned.
    ///
    /// Make sure that `Value` represents a function, or Emacs will signal an error of type `wrong-type-argument`.
    ///
    /// \since Emacs 28
    [[nodiscard]] auto funcFinalizer() const noexcept -> void (*)(void*) EMACS_NOEXCEPT;

    /// Set the function finalizer associated with the module function. It can either be `nullptr` to clear function
    /// finalizer, or a pointer to a function to be called when the object is garbage-collected. At most one function
    /// finalizer can be set per function; if it already has a finalizer, it is replaced.
    ///
    /// Make sure that `Value` represents a function, or Emacs will signal an error of type `wrong-type-argument`.
    ///
    /// \since Emacs 28
    void funcFinalizer(void (*fin)(void*) EMACS_NOEXCEPT) noexcept;
#endif

    /// Call `*this` with args...
    ///
    /// \see Env::call
    template <typename... Args>
    Expected<Value, Error> operator()(Args&&... args) noexcept;

    /// Check whether the Lisp object is not `nil`.
    ///
    /// There can be multiple different values that represent `nil`.
    ///
    /// You could implement an equivalent test by using `intern` to get an `emacs_value` represent `nil`, then use `eq`,
    /// described below, to test for equality. But using this function is more convenient.
    operator bool() const noexcept;

    /// \{
    ///
    /// Check whether `*this` and `rhs` represent the same Lisp object.
    ///
    /// `operator==` corresponds to the Lisp `eq` function. For other kinds of equality comparisons, such as `eql`,
    /// `equal`, use `intern` and `funcall` to call the corresponding Lisp function.
    ///
    /// Two `Value` objects that are different in the C++ sense might still represent the same Lisp object, so you must
    /// always call `operator==` to check for equality.
    bool operator==(const Value& rhs) const noexcept;

    bool operator!=(const Value& rhs) const noexcept {
        return !(*this == rhs);
    }
    /// \}

private:
    // Implementations of `as`
    Expected<std::intmax_t, Error> as(std::integral_constant<Value::Type, Value::Type::Int>) const noexcept;
    Expected<double, Error> as(std::integral_constant<Value::Type, Value::Type::Float>) const noexcept;
    Expected<std::string, Error> as(std::integral_constant<Value::Type, Value::Type::String>) const noexcept;
    Expected<std::chrono::nanoseconds, Error> as(std::integral_constant<Value::Type, Value::Type::Time>) const noexcept;
    Expected<void*, Error> as(std::integral_constant<Value::Type, Value::Type::UserPtr>) const noexcept;

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
    Error(FuncallExit status, Value sym, Value data) noexcept : status_(status), sym_(sym), data_(data) {}

    /// Return the exit status of a `funcall`.
    ///
    /// It shouldn't be `FuncallExit::Return`.
    [[nodiscard]] FuncallExit status() const noexcept {
        return status_;
    }

    /// Report error to Emacs
    void report(Env& env) const noexcept;

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
    [[nodiscard]] Value symbol() const noexcept {
        return sym_;
    }

    [[nodiscard]] Value data() const noexcept {
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
    [[nodiscard]] Value tag() const noexcept {
        return sym_;
    }

    [[nodiscard]] Value value() const noexcept {
        return data_;
    }
    /// \}

private:
    FuncallExit status_;
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

/// Main point of interaction with the Lisp runtime.
///
/// `Env` represents an Emacs module environment. Exported functions and module initializers will receive a valid `Env`
/// value. That `Env` value only remains valid (or "live") when the exported function or module initializer is active.
class Env {
public:
    explicit Env(emacs_env* env) noexcept : env_(env) {}

    /// Return the native handle of `emacs_env`
    [[nodiscard]] emacs_env* native() const noexcept {
        return env_;
    }

    /// Return the canonical symbol whose name is `s`.
    Expected<Value, Error> intern(const char* s) noexcept {
        const emacs_value val = YAPDF_EMACS_APPLY_CHECK(*this, intern, s);
        return Value(val, *this);
    }

    /// \defgroup elisp
    /// \{
    ///
    /// Set `s`'s function definition to `f`.
    ///
    /// \see the Lisp function `defalias`
    Expected<Void, Error> defalias(const char* s, Value f) noexcept {
        const Value symbol = YAPDF_TRY(intern(s));
        return call("defalias", symbol, f).discard();
    }

    /// Provide feature to Emacs.
    ///
    /// \see the Lisp function `provide`
    Expected<Void, Error> provide(const char* feature) noexcept {
        return call("provide", intern(feature)).discard();
    }

    /// Call the Emacs special form `defvar`.
    ///
    /// \see the Lisp `defvar` function
    template <typename T>
    Expected<Void, Error> defvar(const char* sym, T&& init, const char* docstring) noexcept {
        // Can't use `call` because `defvar` is not a function
        return eval(list(intern("defvar"), intern(sym), to_lisp(*this, std::forward<T>(init)), docstring)).discard();
    }

    /// Evaluate using the Emacs function `eval`.
    ///
    /// The binding is always lexical.
    ///
    /// \see the Lisp `eval` function
    template <typename... Args>
    Expected<Value, Error> eval(Args&&... args) noexcept {
        return call("eval", std::forward<Args>(args)..., intern("t"));
    }

    /// Create a list with specified arguments as elements.
    ///
    /// zero argument is allowed too.
    ///
    /// \see the Lisp `list` function
    template <typename... Args>
    Expected<Value, Error> list(Args&&... args) noexcept {
        return call("list", std::forward<Args>(args)...);
    }
    /// \}

    /// Import an Emacs function as a C++ function.
    ///
    /// `sym` must be the Emacs symbol name of the function. Calling the C++ function converts all arguments to Emacs,
    /// calls the Emacs function name.
    ///
    /// \see call
    auto importar(const char* sym) noexcept {
        return
            [*this, sym](auto&&... args) mutable noexcept { return call(sym, std::forward<decltype(args)>(args)...); };
    }

    /// Call a Lisp function f, passing the given arguments.
    ///
    /// - `f` should be a string, a Lisp's callable `Value` or a `GlobalRef`. `std::abort` otherwise.
    /// - `args` should be one of the following types:
    ///   - `std::chrono::nanoseconds`
    ///   - `const char*`
    ///   - `const std::string&`
    ///   - `bool`
    ///   - `double`
    ///   - `void*`
    ///   - `Value`
    ///   - `Expected<Value, Error>` for convenience
    ///   - Integrals
    ///
    /// \see to_lisp
    template <typename F, typename... Args>
    Expected<Value, Error> call(F f, Args&&... args) noexcept {
        emacs_value symbol;
        if constexpr (std::is_same_v<F, Value> || std::is_same_v<F, GlobalRef>) {
            symbol = f.native();
        } else if constexpr (std::is_same_v<F, const char*>) {
            symbol = YAPDF_EMACS_APPLY_CHECK(*this, intern, f);
        } else {
            YAPDF_UNREACHABLE("Unsupported function type");
        }

        constexpr std::size_t n = sizeof...(args);

        emacs_value ys[n];
        Expected<Value, Error> xs[] = {to_lisp(*this, std::forward<Args>(args))...};
        for (std::size_t i = 0; i < n; ++i) {
            if (xs[i].hasError()) {
                return Unexpected(xs[i].error());
            }
            ys[i] = xs[i].value().native();
        }

        return Value(YAPDF_EMACS_APPLY_CHECK(*this, funcall, symbol, n, ys), *this);
    }

    /// Display a message at the bottom of the screen.
    ///
    /// The message also goes into the `*Messages*` buffer. In batch mode, the message is printed to the stderr,
    /// followed by a newline.
    ///
    /// \see the Lisp `message` function
    int message(const char* fmt, ...) noexcept {
        std::va_list ap;

        // determine the required size
        va_start(ap, fmt);
        int n = std::vsnprintf(nullptr, 0, fmt, ap);
        va_end(ap);

        if (n < 0) {
            return -1;
        }

        // one extra byte for '\0'
        const std::size_t sz = n + 1;
        char* p = (char*)std::malloc(sz);
        if (!p) {
            return -1;
        }

        // format
        va_start(ap, fmt);
        n = std::vsnprintf(p, sz, fmt, ap);
        va_end(ap);

        if (n < 0) {
            std::free(p);
            return -1;
        }

        // (message "%s" *p)
        const auto result = call("message", "%s", *p);
        std::free(p);

        return result.hasValue() ? 0 : -1;
    }

    /// Create an Emacs Lisp value from native C++ types.
    ///
    /// # Int
    ///
    /// If the value can't be represented as an Emacs integer, Emacs will signal an error of type `overflow-error`.
    ///
    /// # Float
    ///
    /// Create an Emacs floating-point number from a C++ floating-point value.
    ///
    /// # String
    ///
    /// Create a multibyte Lisp string object.
    ///
    /// If `len` is larger than the maximum allowed Emacs string length, Emacs will raise an `overflow-error` signal.
    /// Otherwise, Emacs treats the memory at `s` as the UTF-8 representation of a string.
    ///
    /// If the memory block delimited by `s` and `len` contains a valid UTF-8 string, the result value will be a
    /// multibyte Lisp string that contains the same sequence of Unicode scalar values as represented by `s`. Otherwise,
    /// the return value will be a multibyte Lisp string with unspecified contents; in practice, Emacs will attempt to
    /// detect as many valid UTF-8 subsequence in `s` as possible and treat the rest as undecodable bytes, but you
    /// shouldn't rely on any specific behavior in this case.
    ///
    /// The returned Lisp string will not contain any text properties. To create a string containing text properties,
    /// use `funcall` to call functions such as `propertize`.
    ///
    /// It can't create strings that contains characters that are not valid Unicode scalar values. Such strings are
    /// rare, but occur from time to time; examples are strings with UTF-16 surrogate code points or strings with
    /// extended Emacs characters that don't correspond to Unicode code points. To create such a Lisp string, call e.g.
    /// the function `string` and pass the desired character values as integers.
    ///
    /// # ByteString
    ///
    /// Create a unibyte Lisp string object.
    ///
    /// It's similar to `String` but has no restrictions on the values of the bytes in the C++ string, and can be used
    /// to pass binary data to Emacs in the form of a unibyte string.
    ///
    /// Emacs has two text ways to represent text in a string or buffer. These are called unibyte and multibyte. Each
    /// string, and each buffer, uses one of these two representations. For most purposes, you can ignore the issue of
    /// representations, because Emacs converts text between them as appropriate. Occasionally in Lisp programming you
    /// will need to pay attention to the difference.
    ///
    /// In unibyte representation, each character occupies one byte and therefore the possible character codes range
    /// from 0 to 255. Codes 0 through 127 are ASCII characters; the codes from 128 through 255 are used for one
    /// non-ASCII character set (you can choose which character set by setting the variable nonascii-insert-offset).
    ///
    /// In multibyte representation, a character may occupy more than one byte, and as a result, the full range of Emacs
    /// character codes can be stored. The first byte of a multibyte character is always in the range 128 through 159
    /// (octal 0200 through 0237). These values are called leading codes. The second and subsequent bytes of a multibyte
    /// character are always in the range 160 through 255 (octal 0240 through 0377); these values are trailing codes.
    ///
    /// Available since Emacs 28.
    ///
    /// # Time
    ///
    /// Create an Emacs timestamp as a pair `(TICKS . HZ)`.
    ///
    /// Available since Emacs 27.
    ///
    /// # UserPtr
    ///
    /// Create a user pointer Lisp object.
    ///
    /// When dealing with C++ code, it's often useful to be able to store arbitrary C++ objects inside Emacs Lisp
    /// objects. For this purpose the module API provides a unique Lisp datatype called user pointer. A user pointer
    /// object encapsulates a C++ pointer value and an optional finalizer function. Apart from storing it, Emacs leaves
    /// the pointer value alone. Even though it's a pointer, there's no requirement that it points to valid memory. If
    /// you provide a finalizer, Emacs will call it when the user pointer object is garbage collected. Note that Emacs's
    /// garbage collection is nondeterministic: it might happen long after an object ceases to be used or not at all.
    /// Therefore you can't use user pointer finalizers for finalization that has to be prompt or deterministic; it's
    /// best to use finalizers only for clean-ups that can be delayed arbitrarily without bad side effects, such as
    /// freeing memory. If you store a resource handle in a user pointer that requires deterministic finalization, you
    /// should use a different mechanism such as unwind-protect. Finalizers can't interact with Emacs in any way; they
    /// also can't fail.
    ///
    /// If fin is not `nullptr`, it must point to a finalizer function with the following signature:
    ///
    /// ``` cpp
    /// void fin (void* ptr) EMACS_NOEXCEPT;
    /// ```
    ///
    /// When the new user pointer object is being garbage collected, Emacs calls fin with ptr as argument. The finalizer
    /// function may contain arbitrary code, but it must not interact with Emacs in any way or exit nonlocally. It
    /// should finish as quickly as possible because delaying garbage collection blocks Emacs completely.
    ///
    /// # Function
    ///
    /// Create an Emacs function from a C++ function.
    ///
    /// This is how you expose functionality from dynamic module to Emacs. To use it, you need to define a module
    /// function and pass its address as the function argument to `make_function`. `min_arity` and `max_arity` must be
    /// nonnegative numbers, and `max_arity` must be greater than or equal to `min_arity`. Alternatively, `max_arity`
    /// can have the special value `emacs_variadic_function`; in this case the function accepts an unbounded number of
    /// arguments, like functions defined with `&rest` in Lisp. The value of `emacs_variadic_function` is a negative
    /// number. When applied to a function object returned by `make_function`, the Lisp function `subr-arity` will
    /// return `(min_arity . max_arity)` if `max_arity` is nonnegative, or `(min_arity . many)` if `max_arity` is
    /// `emacs_variadic_function`.
    ///
    /// Emacs passes the value of the data argument that you give to `make_function` back to your module function, but
    /// doesn't touch it in any other way. You can use data to pass additional context to the module function. If data
    /// points to an object, you are responsible to ensure that the object is still live when Emacs calls the module
    /// function.
    ///
    /// Documentation can either be `nullptr` or a pointer to a null-terminated string. If it's `nullptr`, the new
    /// function won't have a documentation string. If it's not `nullptr`, Emacs interprets it as an UTF-8 string and
    /// uses it as documentation string for the new function. If it's not a valid UTF-8 string, the documentation string
    /// for the new function is unspecified.
    ///
    /// The documentation string can end with a special string to specify the argument names for the function. See
    /// Documentation Strings of Functions in the Emacs Lisp reference manual for the syntax.
    ///
    /// The function returned by `make_function` isn't bound to a symbol. For the common case that you want to create a
    /// function object and bind it to a symbol so that Lisp code can call it by name, you need to call `defalias`
    /// later.
    ///
    /// There are three ways to make a function in this library.
    ///
    /// The first one is as simple as the `make_function`:
    ///
    /// ``` c++
    /// e.make<Function>(min_arity, max_arity, [](emacs_env* env, emacs_value args[], std::ptrdiff_t n, void* data) -> emacs_value {
    ///     // ...
    /// },
    /// "example");
    /// ```
    ///
    /// The second one wraps `emacs_value` in `Value` so that user doesn't need to construct it again from `emacs_value`.
    ///
    /// ``` c++
    /// e.make<Function>(min_arity, max_arity, [](Env& e, Value args[], std::size_t n) -> Expected<Value, Error> {
    ///     assert(n >= min_arity);
    ///     assert(n <= max_arity);
    ///
    ///     const int a = YAPDF_TRY(args[0].as<Value::Type::Int>());
    ///     const int b = YAPDF_TRY(args[1].as<Value::Type::Int>());
    ///     return e.make<Value::Type::Int>(a + b);
    /// },
    /// "example");
    /// ```
    ///
    /// The third one can't specify the arity of function, but it uses C++ native types. No need to convert between C++
    /// native types and Elisp types. The first argument of the lambda must be `Env&` type.
    ///
    /// ``` c++
    /// e.make<Function>(+[](Env& e, int a, int b) { return a + b; }, "add");
    /// ```
    ///
    /// The `+` is necessary, since it makes the lambda decay to function pointer.
    ///
    /// # Rationale
    ///
    /// - For those lambdas that returning `void`, an Elisp `nil` is returned when called in elisp
    /// - The first argument of the lambda must be `Env&` type
    template <Value::Type type, typename... Args>
    Expected<Value, Error> make(Args&&... args) noexcept {
        return make(std::integral_constant<Value::Type, type>{}, std::forward<Args>(args)...);
    }

#if EMACS_MAJOR_VERSION >= 26
    /// Return `true` if the user wants to quit by hitting <kbd>C-g</kbd>.
    ///
    /// In that case, you should return to Emacs as soon as possible, potentially aborting long-running operations. When
    /// a quit is pending after return from a module function, Emacs quits without taking the result value or a possible
    /// pending nonlocal exit into account.
    ///
    /// If your module includes potentially long-running code, it's a good practice to check from time to time in that
    /// code whether the user wants to quit.
    ///
    /// Deprecated: use `processInput` instead if available.
    bool shouldQuit() noexcept {
        return YAPDF_EMACS_APPLY(*this, should_quit);
    }
#endif

#if EMACS_MAJOR_VERSION >= 27
    /// Process pending input events.
    ///
    /// If the user wants to quit or an error occurred while processing signals, if returns `ProcessInputResult::Quit`.
    /// In that case, we recommend that your module function aborts any on-going processing and returns as soon as
    /// possible. If the module code may continue running, it returns `ProcessInputResult::Continue`. The return value
    /// is `ProcessInputResult::Continue` if and only if there is no pending nonlocal exit in `env`. If the module
    /// continues after calling `processInput`, global state such as variable values and buffer content may have been
    /// modified in arbitrary ways.
    ProcessInputResult processInput() noexcept {
        const emacs_process_input_result status = YAPDF_EMACS_APPLY(*this, process_input);
        return static_cast<ProcessInputResult>(status);
    }
#endif

#if EMACS_MAJOR_VERSION >= 28
    /// Open a channel to an existing pipe process.
    ///
    /// `process` must refer to an existing pipe process created by `make-pipe-process`. If successful, the return value
    /// will be a new file descriptor that you can use to write to the pipe. Unlike all other module function, you can
    /// use the returned file descriptor from arbitrary threads, even if no module environment is active. You can use
    /// the `write` function to write to the file descriptor. Once done, close the file descriptor using `close`.
    ///
    /// \since Emacs 28
    Expected<int, Error> openChannel(Value process) noexcept {
        return YAPDF_EMACS_APPLY_CHECK(*this, open_channel, process.native());
    }
#endif

    /// Obtain the last function exit type for an environment.
    ///
    /// It never fails and always returns normally.
    ///
    /// - If there is no nonlocal exit pending, it returns `FuncallExit::Return`.
    /// - If there is a nonlocal raised by the Lisp `signal()` function, it returns `FuncallExit::Signal`
    /// - Otherwise, it returns `FuncallExit::Throw` which is raised by the Lisp `throw()` function
    FuncallExit checkError() noexcept {
        const emacs_funcall_exit status = YAPDF_EMACS_APPLY(*this, non_local_exit_check);
        return static_cast<FuncallExit>(status);
    }

    /// Retrieve additional data for nonlocal exits.
    ///
    /// - It shouldn't be called if there is no nonlocal exit pending, or using the `Error` value returned by this
    /// function is undefined.
    /// - If it's caused by `signal()`, the `Error` value holds the error symbol and relevant data
    /// - If it's caused by `throw()`, the `Error` value holds the tag and the catch value
    Error getError() noexcept {
        emacs_value sym = nullptr, data = nullptr;
        const emacs_funcall_exit status = YAPDF_EMACS_APPLY(*this, non_local_exit_get, &sym, &data);
        return Error(static_cast<FuncallExit>(status), Value(sym, *this), Value(data, *this));
    }

    /// Reset the pending-error state of `Env`.
    ///
    /// After calling `clearError()`, `checkError()` will again return `FuncallExit::Return`, and Emacs won't signal an
    /// error after returning from the current module function or module initialization function.
    ///
    /// You can use `clearError()` to ignore certain kinds of errors.
    void clearError() noexcept {
        YAPDF_EMACS_APPLY(*this, non_local_exit_clear);
    }

    /// `throwError()` is the module equivalent of the Lisp `throw()` function: it causes Emacs to perform a nonlocal
    /// jump to a catch block tagged with `tag`; the catch value will be `val`.
    ///
    /// `throwError()`, like all other environment functions, actually returns normally when seen as a C++ function.
    /// Rather, it causes Emacs to throw to the catch block once you return from the current module function or module
    /// initialization function. Therefore you should typically return quickly after requesting a jump with this
    /// function. If there was already a nonlocal exit pending when calling `throwError()`, the function does nothing;
    /// i.e. it doesn't overwrite catch tag and value. To do that, you must explicitly call `clearError()` first.
    void throwError(Error err) noexcept {
        YAPDF_EMACS_APPLY(*this, non_local_exit_throw, err.tag().native(), err.value().native());
    }

    /// `signalError()` is the module equivalent of the Lisp `signal()` function: it causes Emacs to signal an error of
    /// type `sym` with error data `data`. `data` should be a list.
    ///
    /// `signalError()`, like all other environment functions, actually returns normally when seen as a C++ function.
    /// Rather, it causes Emacs to signal an error once you return from the current module function or module
    /// initialization function. Therefore you should typically return quickly after signaling an error with this
    /// function. If there was already a nonlocal exit pending when calling `signalError()`, the function does nothing;
    /// i.e. it doesn't overwrite the error symbol and data. To do that, you must explicitly call `clearError()` first.
    void signalError(Error err) noexcept {
        YAPDF_EMACS_APPLY(*this, non_local_exit_signal, err.symbol().native(), err.data().native());
    }

private:
    // Implementations of `make`
    template <typename T, YAPDF_REQUIRES(std::is_integral_v<T>)>
    Expected<Value, Error> make(std::integral_constant<Value::Type, Value::Type::Int>, T x) noexcept {
        return Value(YAPDF_EMACS_APPLY_CHECK(*this, make_integer, x), *this);
    }

    template <typename T, YAPDF_REQUIRES(std::is_floating_point_v<T>)>
    Expected<Value, Error> make(std::integral_constant<Value::Type, Value::Type::Float>, T x) noexcept {
        return Value(YAPDF_EMACS_APPLY_CHECK(*this, make_float, x), *this);
    }

    Expected<Value, Error> make(std::integral_constant<Value::Type, Value::Type::Time> tag,
                                std::chrono::nanoseconds ns) noexcept {
        const auto secs = std::chrono::duration_cast<std::chrono::seconds>(ns);
        return make(tag, secs, ns - secs);
    }

    Expected<Value, Error> make(std::integral_constant<Value::Type, Value::Type::String>, const char* s,
                                std::size_t len) noexcept {
        return Value(YAPDF_EMACS_APPLY_CHECK(*this, make_string, s, len), *this);
    }

    Expected<Value, Error> make(std::integral_constant<Value::Type, Value::Type::String> tag, const char* s) noexcept {
        return make(tag, s, std::strlen(s));
    }

    Expected<Value, Error> make(std::integral_constant<Value::Type, Value::Type::String> tag,
                                const std::string& s) noexcept {
        return make(tag, s.c_str(), s.size());
    }

    Expected<Value, Error> make(std::integral_constant<Value::Type, Value::Type::Time>, std::chrono::seconds secs,
                                std::chrono::nanoseconds ns) noexcept {
#if EMACS_MAJOR_VERSION >= 27
        const struct timespec ts = {
            .tv_sec = secs.count(),
            .tv_nsec = ns.count(),
        };
        return Value(YAPDF_EMACS_APPLY_CHECK(*this, make_time, ts), *this);
#else
        YAPDF_UNREACHABLE("make_time: unsupported on current Emacs version");
#endif
    }

    Expected<Value, Error> make(std::integral_constant<Value::Type, Value::Type::ByteString>, const char* s,
                                std::size_t len) noexcept {
#if EMACS_MAJOR_VERSION >= 28
        return Value(YAPDF_EMACS_APPLY_CHECK(*this, make_unibyte_string, s, len), *this);
#else
        YAPDF_UNREACHABLE("make_unibyte_string: unsupported on current Emacs version");
#endif
    }

    Expected<Value, Error> make(std::integral_constant<Value::Type, Value::Type::ByteString> tag,
                                const std::string& s) noexcept {
        return make(tag, s.c_str(), s.size());
    }

    Expected<Value, Error> make(std::integral_constant<Value::Type, Value::Type::UserPtr>, void* p,
                                void (*fin)(void*) EMACS_NOEXCEPT = nullptr) noexcept {
        return Value(YAPDF_EMACS_APPLY_CHECK(*this, make_user_ptr, fin, p), *this);
    }

    static emacs_value trampoline(emacs_env* env, std::ptrdiff_t nargs, emacs_value args[], void* data) EMACS_NOEXCEPT {
        Env e(env);
        const auto f = reinterpret_cast<Expected<Value, Error> (*)(Env&, Value[], std::size_t)>(data);
        try {
            std::vector<Value> vs;
            vs.reserve(nargs);
            for (int i = 0; i < nargs; ++i) {
                vs.emplace_back(args[i], e);
            }

            const Expected<Value, Error> result = f(e, vs.data(), vs.size());
            if (YAPDF_LIKELY(result.hasValue())) {
                return result.value().native();
            }

            result.error().report(e);
        } catch (const std::overflow_error& ex) {
            signal(env, "overflow-error", ex.what());
        } catch (const std::underflow_error& ex) {
            signal(env, "underflow-error", ex.what());
        } catch (const std::range_error& ex) {
            signal(env, "range-error", ex.what());
        } catch (const std::out_of_range& ex) {
            signal(env, "out-of-range", ex.what());
        } catch (const std::bad_alloc& ex) {
            signal(env, "memory-full", ex.what());
        } catch (const std::exception& ex) {
            // If you have more exception types that you'd like to treat specially, add handlers for them here.
            signal(env, "error", ex.what());
        } catch (...) {
            signal(env, "error", "unknown error");
        }
        return nullptr;
    }

    template <typename... Args, typename F, std::size_t... Is>
    static auto trampoline(F&& f, Env& e, emacs_value args[], std::index_sequence<Is...>) {
        // `.value()` will throw if it can't construct `Args` from Elisp types
        return std::invoke(std::forward<F>(f), e, from_lisp<Args>(e, args[Is]).value()...);
    }

    // The arguments of f can be `Value` type, and any calls of `Env` can yield `Error` result, returning an
    // `Expected<Value, Error>` makes sense
    Expected<Value, Error> make(std::integral_constant<Value::Type, Value::Type::Function> tag,
                                std::ptrdiff_t min_arity,                                // must be greater than zero
                                std::ptrdiff_t max_arity,                                // `emacs_variadic_function`
                                Expected<Value, Error> (*f)(Env&, Value[], std::size_t), // it can throw exception
                                const char* docstring) noexcept {                        // the docstring of function
        return make(tag, min_arity, max_arity, trampoline, docstring, reinterpret_cast<void*>(f));
    }

    Expected<Value, Error> make(std::integral_constant<Value::Type, Value::Type::Function>,
                                std::ptrdiff_t min_arity, // must be greater than zero
                                std::ptrdiff_t max_arity, // `emacs_variadic_function`
                                EmacsFunction f,          // must not throw exception
                                const char* docstring,    // the docstring of function
                                void* data) noexcept {    // the extra data will be passed with f
        return Value(YAPDF_EMACS_APPLY_CHECK(*this, make_function, min_arity, max_arity, f, docstring, data), *this);
    }

    template <typename R, typename... Args>
    Expected<Value, Error> make(std::integral_constant<Value::Type, Value::Type::Function> tag,
                                R (*f)(Env& e, Args... args), const char* docstring) noexcept {
        constexpr std::size_t arity = sizeof...(Args);
        return make(
            tag, arity, arity,
            [](emacs_env* env, std::ptrdiff_t nargs, emacs_value args[], void* data) EMACS_NOEXCEPT -> emacs_value {
                assert(arity == nargs);

                Env e(env);
                const auto f = reinterpret_cast<R (*)(Env&, Args...)>(data);
                try {
                    if constexpr (std::is_void_v<R>) {
                        Env::trampoline<Args...>(f, e, args, std::index_sequence_for<Args...>{});
                        return env->intern(env, "nil");
                    } else {
                        const auto result = Env::trampoline<Args...>(f, e, args, std::index_sequence_for<Args...>{});
                        if (const auto ex = to_lisp(e, result); YAPDF_LIKELY(ex.hasValue())) {
                            return ex.value().native();
                        } else {
                            ex.error().report(e);
                        }
                    }
                } catch (const std::overflow_error& ex) {
                    signal(env, "overflow-error", ex.what());
                } catch (const std::underflow_error& ex) {
                    signal(env, "underflow-error", ex.what());
                } catch (const std::range_error& ex) {
                    signal(env, "range-error", ex.what());
                } catch (const std::out_of_range& ex) {
                    signal(env, "out-of-range", ex.what());
                } catch (const std::bad_alloc& ex) {
                    signal(env, "memory-full", ex.what());
                } catch (const BadExpectedAccess& ex) {
                    signal(env, "convert-error", ex.what());
                } catch (const std::exception& ex) {
                    signal(env, "error", ex.what());
                } catch (...) {
                    signal(env, "error", "unknown error");
                }
                return nullptr;
            },
            docstring, reinterpret_cast<void*>(f));
    }

    /// Signal an error to Emacs.
    ///
    /// Avoid to call C++ functions since exceptions are inhibited in FFI boundary.
    static void signal(emacs_env* env, const char* sym, const char* what) noexcept {
        // It's rare to cause an error during `make_string` and `intern`. `non_local_exit_signal` will tell us the
        // allocation failures
        emacs_value what_obj = env->make_string(env, what, std::strlen(what));
        emacs_value data = env->funcall(env, env->intern(env, "list"), 1, &what_obj);
        env->non_local_exit_signal(env, env->intern(env, sym), data);
    }

    // Convert a C++ type to Lisp `Value`
    //
    // It converts
    //
    // - `std::chrono::nanoseconds` to `Value::Type::Time`
    // - `const char*` and `const std::string&` to `Value::Type::String`
    // - `double` to `Value::Type::Float`
    // - `void*` to `Value::Type::UserPtr`, e.g. `int*` can implicitly cast to `void*`
    // - `Value` to `Value`
    // - `bool` to `t` or `nil`
    // - Others to `Value::Type::Int`
    //
    // Since `Env` is still incomplete, we use `auto&` to delay the requirement of `Env` to be a complete type.
    inline static constexpr auto to_lisp = Overload{
        [](auto&, Value x) -> Expected<Value, Error> { return x; },
        [](auto& e, GlobalRef x) -> Expected<Value, Error> { return x.bind(e); },
        [](auto&, Expected<Value, Error> ex) -> Expected<Value, Error> { return ex; },
        [](auto& e, bool b) -> Expected<Value, Error> { return e.intern(b ? "t" : "nil"); },
        [](auto& e, void* p) -> Expected<Value, Error> { return e.template make<Value::Type::UserPtr>(p); },
        [](auto& e, float x) -> Expected<Value, Error> { return e.template make<Value::Type::Float>(x); },
        [](auto& e, double x) -> Expected<Value, Error> { return e.template make<Value::Type::Float>(x); },
        [](auto& e, const char* s) -> Expected<Value, Error> { return e.template make<Value::Type::String>(s); },
        [](auto& e, const std::string& s) -> Expected<Value, Error> { return e.template make<Value::Type::String>(s); },
        [](auto& e, std::chrono::nanoseconds ns) -> Expected<Value, Error> {
            return e.template make<Value::Type::Time>(ns);
        },
        [](auto& e, auto x) -> Expected<Value, Error> {
            static_assert(std::is_integral_v<decltype(x)>, "x must be integral types");
            return e.template make<Value::Type::Int>(x);
        },
    };

    // Convert an `emacs_value` to C++ type.
    //
    // - `std::chrono::nanoseconds` from `Value::Type::Time`
    // - `std::string` from `Value::Type::String`
    // - `double`, `float` and other floating point types from `Value::Type::Float`
    // - `void*` from `Value::Type::UserPtr`
    // - `bool` by checking `is_not_nil` of `emacs_value`
    // - `Value` identical to `Value`
    // - integral types from `Value::Type::Int`
    template <typename T>
    inline static constexpr auto from_lisp = [](Env& e, emacs_value v) -> Expected<T, Error> {
        if constexpr (std::is_same_v<T, std::chrono::nanoseconds>) {
            return Value(v, e).as<Value::Type::Time>();
        } else if constexpr (std::is_same_v<T, std::string>) {
            return Value(v, e).as<Value::Type::String>();
        } else if constexpr (std::is_floating_point_v<T>) {
            return Value(v, e).as<Value::Type::Float>().map([](auto x) { return static_cast<T>(x); });
        } else if constexpr (std::is_pointer_v<T>) {
            return Value(v, e).as<Value::Type::UserPtr>();
        } else if constexpr (std::is_same_v<T, bool>) {
            return bool(Value(v, e));
        } else if constexpr (std::is_same_v<T, Value>) {
            return Value(v, e);
        } else if constexpr (std::is_integral_v<T>) {
            return Value(v, e).as<Value::Type::Int>().map([](auto x) { return static_cast<T>(x); });
        } else {
            YAPDF_UNREACHABLE("Handle more types");
        }
    };

    emacs_env* env_;
};

template <typename R, typename... Args>
inline void DefunUniversalFunction<R, Args...>::def(Env& e) noexcept {
    const Value fn = e.make<Value::Type::Function>(f_, docstring_).expect(name_);
    e.defalias(name_, fn).expect(name_);
}

inline Expected<std::intmax_t, Error> Value::as(std::integral_constant<Value::Type, Value::Type::Int>) const noexcept {
    const std::intmax_t val = YAPDF_EMACS_APPLY_CHECK(env_, extract_integer, val_);
    return val;
}

inline Expected<double, Error> Value::as(std::integral_constant<Value::Type, Value::Type::Float>) const noexcept {
    const double val = YAPDF_EMACS_APPLY_CHECK(env_, extract_float, val_);
    return val;
}

inline Expected<std::string, Error> Value::as(std::integral_constant<Value::Type, Value::Type::String>) const noexcept {
    // retrieve the length of string
    std::ptrdiff_t len;
    YAPDF_EMACS_APPLY_CHECK(env_, copy_string_contents, val_, nullptr, &len);

    // copy
    std::string s(len, '\0');
    YAPDF_EMACS_APPLY_CHECK(env_, copy_string_contents, val_, &s[0], &len);

    // remove the trailing '\0'
    if (!s.empty()) {
        assert(s.back() == '\0');
        s.pop_back();
    }
    return s;
}

inline Expected<std::chrono::nanoseconds, Error>
Value::as(std::integral_constant<Value::Type, Value::Type::Time>) const noexcept {
#if EMACS_MAJOR_VERSION >= 27
    const struct timespec ts = YAPDF_EMACS_APPLY_CHECK(env_, extract_time, val_);
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(ts.tv_sec) +
                                                                std::chrono::nanoseconds(ts.tv_nsec));
#else
    YAPDF_UNREACHABLE("extract_time: unsupported on current Emacs version");
#endif
}

inline Expected<void*, Error> Value::as(std::integral_constant<Value::Type, Value::Type::UserPtr>) const noexcept {
    return YAPDF_EMACS_APPLY_CHECK(env_, get_user_ptr, val_);
}

template <typename... Args>
inline Expected<Value, Error> Value::operator()(Args&&... args) noexcept {
    return env_.call(*this, std::forward<Args>(args)...);
}
} // namespace emacs
} // namespace yapdf

#endif // YAPDF_BRIDGE_HPP_
