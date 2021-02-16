//! A utility class to represent an expected monad.
//!
//! There are several techniques to report and handle failures in functions.
//!
//! # Exception
//!
//! Exception is the default mechanism in C++ for reporting, propagating and processing failures. The main advantage is
//! the ability to describe the success dependency between functions: if you want to say that calling function `g()`
//! depends on the successful execution of function `f()`, you just put `g()` below `f()` and that's it:
//!
//! ``` cpp
//! int foo() {
//!     f();
//!     g();        // don't call g() and further if f() fails
//!     return h(); // don't call h() if g() fails
//! }
//! ```
//!
//! In a more pedantic term this means that `f()` is sequenced before `g()`. This makes failure handling easy: in a lot
//! of cases you don't have to do anything.
//!
//! Also, while next operations are being canceled, the exception object containing the information about the initial
//! failure is kept on the side. When at some point the cancellation cascade is stopped by an exception handler, the
//! exception object can be inspected. It can contain arbitrarily big amount of data about the failure reason, including
//! the entire call stack.
//!
//! Sadly, there are two kinds of overheads caused by the exception handling mechanism. The first is connected with
//! storing the exceptions on the side. Stack unwinding works independently in each thread of execution; each thread can
//! be potentially handling a number of exceptions (even though only one exception can be active in one thread). This
//! requires preparation for storing an arbitrary number of exceptions of arbitrary types per thread. Additional things
//! like jump tables also need to be stored in the program binaries.
//!
//! The second overhead is experienced when throwing an exception and trying to find the handler. Since nearly any
//! function can throw an exception of any time, this is a dynamic memory allocation. The type of an exception is erased
//! and RTTI is required to access the type of the active exception object. The worst case time required for matching
//! exceptions against handlers cannot be easily predicted and therefore exceptions are not suitable for real-time or
//! low-latency systems.
//!
//! Another problem connected with exceptions is that while they are good for program flows with linear "success
//! dependency", they become inconvenient in situation where this success dependency does not occur. One such notable
//! example is releasing acquired resources which needs to be performed even if previous operations have failed. Another
//! example is when implementing a strong exception safety guarantee we may need to apply some feedback actions when
//! previous operations have failed. When failures are reported by exceptions, the semantics of cancelling all
//! subsequent operations is a hindrance rather than help; there situations require special and nontrivial idioms to be
//! employed.
//!
//! For these reasons in some projects using exceptions is forbidden. Compilers offer switches to disable exceptions
//! altogether (they refuse to compile a throw, and turn already compiled throws into calls to `std::abort()`).
//!
//! # Errno
//!
//! The idiom of returning, upon failure, a special value and storing an error code inside a global (actually
//! thread-local) object `errno` is inherited from C, and used in its standard library:
//!
//! ``` cpp
//! int socketRead(int fd, char buf[1024]) {
//!     int ret = ::read(fd, buf, 1024);
//!     if (ret == -1) {
//!         if (errno == EAGAIN || errno == EWOULDBLOCK) {
//!             // try again the next time
//!         } else {
//!             return 0; // a real error, zero indicates that the remote socket closed
//!         }
//!     }
//!
//!     errno = 0; // success
//!     return ret;
//! }
//! ```
//!
//! One advantage of this technique is that it uses `if` and `return` to indicate all execution paths that handle
//! failures. No implicit control flow happens.
//!
//! Because of the existence of `errno`, our functions are not pure: they have side effects. This means many useful
//! compiler optimizations (like common sub-expression elimination) cannot be applied.
//!
//! Additionally, error propagation using if statements and early returns is manual. We can easily forget to check for
//! the failure, and incorrectly let the subsequent operations execute, potentially causing damage to the program state.
//!
//! # Error Code
//!
//! Error codes are reasonable error handling technique, also working in C. In this case the information is also stored
//! in an `int`, but returned by value, which makes it possible to make functions pure (side-effect-free and
//! referentially transparent).
//!
//! ``` cpp
//! int parseInt(const char* buf, size_t len, int& result) {
//!     if (len == 0) {
//!         return FORMAT_ERROR; // my error code
//!     }
//!     if (!isdigit(buf[0]) && buf[0] != '-' && buf[0] != '+') {
//!         return FORMAT_ERROR; // my error code
//!     }
//!
//!     // ...
//!     return OK; // success
//! }
//! ```
//!
//! Because the type of the error type is known statically, no memory allocation or type erasure is required. This
//! technique is very efficient.
//!
//! All failure paths written manually can be considered both an advantage and a disadvantage. If I need to substitute
//! an error code returned by lower-level function with mine more appropriate at this level, the information about the
//! original failure is gone.
//!
//! Also, all possible error codes invented by different programmers in different third party libraries must fit into
//! one int and not overlap with any other error code value. This is quite impossible and does not scale well.
//!
//! Because errors are communicated through returned values, we cannot use function’s return type to return computed
//! values. Computed values are written to function output parameters, which requires objects to be created before we
//! have values to put into them. This requires many objects in unintended state to exist. Writing to output parameters
//! often requires an indirection and can incur some run-time cost.
//!
//! # `std::error_code`
//!
//! Type `std::error_code` has been designed to be sufficiently small and trivial to be cheaply passed around, and at
//! the same time be able to store sufficient information to represent any error situation from any library/sub-system
//! in the world without a clash. Its representation is basically:
//!
//! ``` cpp
//! class error_code {
//!     error_category* domain; // domain from which the error originates
//!     int             value;  // numeric value of error within the domain
//! };
//! ```
//!
//! Here, `domain` indicates the library from which the error originates. It is a pointer to a global object
//! representing a given library/domain. Different libraries will be represented by different globals. Each domain is
//! expected to be represented by a global object derived from `std::error_category`.
//!
//! Now, value represents a numeric value of a particular error situation within the domain. Thus, different domains can
//! use the same numeric value 1 to indicate different error situations, but two `std::error_code` objects will be
//! different because the pointers representing domains will be different.
//!
//! `std::error_code` comes with additional tools: a facility for defining custom domains with their set of error codes,
//! and a facility for building predicates that allow classifying errors.
//!
//! Once created and passed around (either inside a thrown exception or returned from functions by value) there is never
//! a need to change the value of `error_code` object at any level. But at different levels one can use different
//! predicates for classifying error situations appropriately to the program layer.
//!
//! When a new library needs to represent its own set of error situations in an `error_code` it first has to declare the
//! list of numeric value as an enumeration:
//!
//! ``` cpp
//! enum class ConvertErrc {
//!     StringTooLong = 1, // 0 should not represent an error
//!     EmptyString   = 2,
//!     IllegalChar   = 3,
//! };
//! ```
//!
//! Then it has to put some boiler-plate code to plug the new enumeration into the `std::error_code` system.
//!
//! ``` cpp
//! namespace std {
//! template <>
//! struct is_error_code_enum<ConvertErrc> : std::true_type {};
//! }
//!
//! // Or use your own category if you want
//! std::error_code make_error_code(ConvertErrc errc) {
//!     return {static_cast<int>(errc), std::generic_cateogry()};
//! }
//! ```
//!
//! Then, it can use the enum as an `error_code`:
//!
//! ``` cpp
//! const std::error_code ec = ConvertErrc::EmptyString;
//! assert(ec == ConvertErrc::EmptyString);
//! ```
//!
//! Member value is mapped directly from the numeric value in the enumeration, and member domain is mapped from the type
//! of the enumeration. Thus, this is a form of type erasure, but one that does allow type `std::error_code` to be
//! trivial and standard-layout.
//!
//! SPDX-License-Identifier: GPL-2.0-or-later

#ifndef YAPDF_EXPECTED_HPP_
#define YAPDF_EXPECTED_HPP_

#include <functional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

#include "likely.hpp"
#include "requires.hpp"

#define YAPDF_TRY(exp)                                                                                                 \
    ({                                                                                                                 \
        const auto result = (exp);                                                                                     \
        if (YAPDF_UNLIKELY(result.hasError())) {                                                                       \
            return std::move(result.error());                                                                          \
        }                                                                                                              \
        std::move(result.value())                                                                                      \
    })

namespace yapdf {
/// An exception type to report the situation where an attempt to access the value of `Expected<T, E>` object that
/// contains an `Unexpected<E>`.
///
/// # Example
///
/// ``` cpp
/// Expected<int, bool> ex(Unexpect, false);
/// ex.value(); // throws BadExpectedAccess
/// ```
class BadExpectedAccess : public std::logic_error {
public:
    explicit BadExpectedAccess(const char* s = "Bad expected access") : std::logic_error(s) {}
};

/// A helper type used to disambiguate the construction of `Expected<T, E>` in the error state
template <typename E>
class Unexpected final {
    static_assert(!std::is_same_v<E, void>, "E must not be void type");

public:
    /// Initialize as if direct-non-list-initializing an object of type `E` with the arguments
    /// `std::forward<Args>(args)...`
    template <typename... Args, YAPDF_REQUIRES(std::is_constructible_v<E, Args...>)>
    explicit Unexpected(std::in_place_t, Args&&... args) noexcept(std::is_nothrow_constructible_v<E, Args...>)
        : error_(std::forward<Args>(args)...) {}

    /// Initialize as if copy construct an object of type `E`
    explicit Unexpected(const E& e) noexcept(std::is_nothrow_copy_constructible_v<E>) : error_(e) {}

    /// Initialize as if move construct an object of type `E`
    explicit Unexpected(E&& e) noexcept(std::is_nothrow_move_constructible_v<E>) : error_(std::move(e)) {}

    /// Copy construct from another `Unexpected` type
    template <typename U, YAPDF_REQUIRES(std::is_constructible_v<E, U>)>
    /* implicit */ Unexpected(const Unexpected<U>& rhs) noexcept(std::is_nothrow_constructible_v<E, U>)
        : error_(rhs.error_) {}

    /// Move construct from another `Unexpected` type
    template <typename U, YAPDF_REQUIRES(std::is_constructible_v<E, U&&>)>
    /* implicit */ Unexpected(Unexpected<U>&& rhs) noexcept(std::is_nothrow_constructible_v<E, U&&>)
        : error_(std::move(rhs.error_)) {}

    /// The default copy constructor
    /* implicit */ Unexpected(const Unexpected&) noexcept(std::is_nothrow_copy_constructible_v<E>) = default;

    /// The default move constructor
    /* implicit */ Unexpected(Unexpected&&) noexcept(std::is_nothrow_move_constructible_v<E>) = default;

    /// Copy assignment from another `Unexpected` type
    template <typename U, YAPDF_REQUIRES(std::is_assignable_v<E, U>)>
    Unexpected& operator=(const Unexpected<U>& rhs) noexcept(std::is_nothrow_assignable_v<E, U>) {
        error_ = rhs.error_;
        return *this;
    }

    /// Move assignment from another `Unexpected` type
    template <typename U, YAPDF_REQUIRES(std::is_assignable_v<E, U&&>)>
    Unexpected& operator=(Unexpected<U>&& rhs) noexcept(std::is_nothrow_assignable_v<E, U&&>) {
        error_ = std::move(rhs.error_);
        return *this;
    }

    /// The default copy assignment operator
    Unexpected& operator=(const Unexpected&) noexcept(std::is_nothrow_copy_assignable_v<E>) = default;

    /// The default move assignment operator
    Unexpected& operator=(Unexpected&&) noexcept(std::is_nothrow_move_assignable_v<E>) = default;

    /// \{
    ///
    /// Yield the contained error
    const E& error() const& noexcept {
        return error_;
    }

    E& error() & noexcept {
        return error_;
    }

    const E&& error() const&& noexcept {
        return std::move(error_);
    }

    E&& error() && noexcept {
        return std::move(error_);
    }
    /// \}

    /// Swap *this with `other`
    void swap(Unexpected& rhs) noexcept(std::is_nothrow_swappable_v<E>) {
        using std::swap;
        swap(error_, rhs.error_);
    }

private:
    E error_;
};

template <typename E>
Unexpected(E) -> Unexpected<E>;

/// \{
///
/// Equality operators for `Unexpected`
template <typename E>
bool operator==(const Unexpected<E>& lhs, const Unexpected<E>& rhs) noexcept {
    return lhs.error() == rhs.error();
}

template <typename E>
bool operator!=(const Unexpected<E>& lhs, const Unexpected<E>& rhs) noexcept {
    return !(lhs == rhs);
}
/// \}

/// Swap two `Unexpected` types
template <typename E>
void swap(Unexpected<E>& lhs, Unexpected<E>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
    lhs.swap(rhs);
}

struct UnexpectType {
    constexpr UnexpectType() noexcept = default;
};
inline constexpr UnexpectType unexpect{};

/// `Expected<T, E>` is a template class that represents either `T` or `E`.
///
/// # Example
///
/// ``` cpp
/// enum class Error {
///     OutOfRange,
///     InvalidFormat,
/// };
///
/// Expected<int, Error> atoi(const char* s) {
///     if (s == '*') {
///         return Unexpected(Error::InvalidFormat);
///     }
///
///     // ...
///     return s[0] - '0';
/// }
///
/// const Expected<int, Error> result = atoi("1");
/// if (result.hasValue()) {
///     printf("%d\n", result.value());
/// } else {
///     printf("Parse Error\n");
/// }
/// ```
///
template <typename T, typename E>
class [[nodiscard]] Expected final {
    static_assert(!std::is_reference_v<T>, "T must not be a reference");
    static_assert(!std::is_reference_v<E>, "E must not be a reference");
    static_assert(!std::is_same_v<std::remove_cv_t<T>, UnexpectType>, "T must not be UnexpectType");
    static_assert(!std::is_same_v<std::remove_cv_t<T>, Unexpected<E>>, "T must not be Unexpected<E>");
    static_assert(!std::is_same_v<std::remove_cv_t<T>, std::in_place_t>, "T must not be std::in_place_t");

public:
    using ValueType = T;
    using ErrorType = E;

    /// Construct `Expected` in successful state
    template <typename... Args>
    explicit Expected(std::in_place_t, Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
        : ex_(std::in_place_index_t<0>{}, std::forward<Args>(args)...) {}

    /// Construct `Expected` in failed state
    template <typename... Args>
    explicit Expected(UnexpectType, Args&&... args) noexcept(std::is_nothrow_constructible_v<E, Args...>)
        : ex_(std::in_place_index_t<1>{}, std::forward<Args>(args)...) {}

    /// \{
    ///
    /// Construct `Expected` with a `T`.
    /* implicit */ Expected(const T& rhs) noexcept(std::is_nothrow_copy_constructible_v<T>)
        : ex_(std::in_place_index_t<0>{}, rhs) {}
    /* implicit */ Expected(T&& rhs) noexcept(std::is_nothrow_move_constructible_v<T>)
        : ex_(std::in_place_index_t<0>{}, std::move(rhs)) {}
    /// \}

    /// \{
    ///
    /// Construct `Expected` with an `E`.
    /* implicit */ Expected(const Unexpected<E>& rhs) noexcept(std::is_nothrow_copy_constructible_v<E>)
        : ex_(std::in_place_index_t<1>{}, rhs.error()) {}
    /* implicit */ Expected(Unexpected<E>&& rhs) noexcept(std::is_nothrow_move_constructible_v<E>)
        : ex_(std::in_place_index_t<1>{}, std::move(rhs.error())) {}
    /// \}

    /// The default copy constructor
    /* implicit */ Expected(const Expected&) noexcept(
        std::is_nothrow_copy_constructible_v<T> && std::is_nothrow_copy_constructible_v<E>) = default;

    /// The default move constructor
    /* implicit */ Expected(Expected&&) noexcept(
        std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_constructible_v<E>) = default;

    /// \{
    ///
    /// Assign a `T` to `Expected`
    Expected& operator=(const T& rhs) noexcept(std::is_nothrow_constructible_v<T>) {
        ex_.template emplace<0>(rhs);
        return *this;
    }
    Expected& operator=(T&& rhs) noexcept(std::is_nothrow_move_constructible_v<T>) {
        ex_.template emplace<0>(std::move(rhs));
        return *this;
    }
    /// \}

    /// \{
    ///
    /// Assign an `Unexpected` to `Expected`
    Expected& operator=(const Unexpected<E>& rhs) noexcept(std::is_nothrow_constructible_v<E>) {
        ex_.template emplace<1>(rhs.error());
        return *this;
    }
    Expected& operator=(Unexpected<E>&& rhs) noexcept(std::is_nothrow_move_constructible_v<E>) {
        ex_.template emplace<1>(std::move(rhs.error()));
        return *this;
    }
    /// \}

    /// The default copy assignment operator
    Expected& operator=(const Expected&) noexcept(
        std::is_nothrow_copy_assignable_v<T> && std::is_nothrow_copy_assignable_v<E>) = default;

    /// The default move assignment operator
    Expected& operator=(Expected&&) noexcept(
        std::is_nothrow_move_assignable_v<T> && std::is_nothrow_move_assignable_v<E>) = default;

    /// Construct from args in place
    ///
    /// ``` cpp
    /// Expected<std::string, int> x(unexpect, 2);
    /// assert(x.hasError() && x.error() == 2);
    ///
    /// x.emplace("foo");
    /// assert(x.hasValue() && x.value() == "foo");
    /// ```
    template <typename... Args>
    T& emplace(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) {
        ex_.template emplace<0>(std::forward<Args>(args)...);
        return std::get<0>(ex_);
    }

    /// Return `true` if it's in successful state
    [[nodiscard]] bool hasValue() const noexcept {
        return ex_.index() == 0;
    }

    /// Return `true` if it's in failed state
    [[nodiscard]] bool hasError() const noexcept {
        return ex_.index() == 1;
    }

    /// Return `true` if the `Expected` object is in successful state containing the given value
    ///
    /// ``` cpp
    /// const Expected<int, int> x(2);
    /// assert(x.contains(2));
    /// assert(!x.contains(3));
    /// ```
    bool contains(const T& x) const noexcept {
        return hasValue() && std::get<0>(ex_) == x;
    }

    /// Return `true` if the `Expected` object is in failed state containing the given value
    ///
    /// ``` cpp
    /// const Expected<int, int> x(unexpect, 3);
    /// assert(x.containsErr(3));
    /// ```
    bool containsErr(const E& x) const noexcept {
        return hasError() && std::get<1>(ex_) == x;
    }

    /// \{
    ///
    /// Map an `Expected<T, E>` to `Expected<U, E>` by applying a function to the contained `T`, leaving an `E`
    /// untouched.
    ///
    /// This function can be used to compose the results of two functions.
    ///
    /// ``` cpp
    /// const auto result = atoi("123").map([](int x) { return x + 1; });
    /// assert(result.hasValue() && result.value() == 124);
    /// ```
    template <typename F, typename U = std::invoke_result_t<F, T>>
    Expected<U, E> map(F&& f) & {
        if (YAPDF_LIKELY(hasValue())) {
            return std::invoke(std::forward<F>(f), value());
        }
        return Unexpected(error());
    }

    template <typename F, typename U = std::invoke_result_t<F, T>>
    Expected<U, E> map(F&& f) const& {
        if (YAPDF_LIKELY(hasValue())) {
            return std::invoke(std::forward<F>(f), value());
        }
        return Unexpected(error());
    }

    template <typename F, typename U = std::invoke_result_t<F, T>>
    Expected<U, E> map(F&& f) && {
        if (YAPDF_LIKELY(hasValue())) {
            return std::invoke(std::forward<F>(f), std::move(value()));
        }
        return Unexpected(std::move(error()));
    }

    template <typename F, typename U = std::invoke_result_t<F, T>>
    Expected<U, E> map(F&& f) const&& {
        if (YAPDF_LIKELY(hasValue())) {
            return std::invoke(std::forward<F>(f), std::move(value()));
        }
        return Unexpected(std::move(error()));
    }
    /// \}

    /// \{
    ///
    /// Map an `Expected<T, E>` to `Expected<T, U>` by applying a function to the contained `E`, leaving the `T`
    /// untouched.
    ///
    /// This function can be used to compose the results of two functions.
    ///
    /// ``` cpp
    /// string stringify(int x) {
    ///     char buf[32] = "";
    ///     auto len = snprintf(buf, sizeof(buf), "%d", x);
    ///     return string(buf, len);
    /// }
    ///
    /// Expected<int, int> e1 = 2;
    /// const auto result1 = e1.mapErr(stringify);
    /// assert(result1.hasValue() && result1.value() == 2);
    ///
    /// Expected<int, int> e2 = Unexpected(2);
    /// const auto result2 = e2.mapErr(stringify);
    /// assert(result2.hasError() && result2.error() == "2");
    /// ```
    template <typename F, typename U = std::invoke_result_t<F, E>>
    Expected<T, U> mapErr(F&& f) & {
        if (YAPDF_LIKELY(hasError())) {
            return Unexpected(std::invoke(std::forward<F>(f), error()));
        }
        return value();
    }

    template <typename F, typename U = std::invoke_result_t<F, E>>
    Expected<T, U> mapErr(F&& f) const& {
        if (YAPDF_LIKELY(hasError())) {
            return Unexpected(std::invoke(std::forward<F>(f), error()));
        }
        return value();
    }

    template <typename F, typename U = std::invoke_result_t<F, E>>
    Expected<T, U> mapErr(F&& f) && {
        if (YAPDF_LIKELY(hasError())) {
            return Unexpected(std::invoke(std::forward<F>(f), std::move(error())));
        }
        return std::move(value());
    }

    template <typename F, typename U = std::invoke_result_t<F, E>>
    Expected<T, U> mapErr(F&& f) const&& {
        if (YAPDF_LIKELY(hasError())) {
            return Unexpected(std::invoke(std::forward<F>(f), std::move(error())));
        }
        return std::move(value());
    }
    /// \}

    /// \{
    ///
    /// Call `f` if the `Expected` holds a `T`, otherwise return the `E`.
    ///
    /// This function can be used for control flow based on `Expected`.
    ///
    /// ``` cpp
    /// const Expected<int, int> e1 = 2;
    /// assert(e1.andThen([](int x) -> Expected<int, int> { return x * x; }).value(), 4);
    ///
    /// const Expected<int, int> e2 = Unexpected(3);
    /// assert(e2.andThen([](int x) -> Expected<int, int> { return x + 1; }).error(), 3);
    /// ```
    template <typename F, typename R = std::invoke_result_t<F, T>, typename U = typename R::ValueType,
              YAPDF_REQUIRES(std::is_same_v<R, Expected<U, E>>)>
    Expected<U, E> andThen(F&& f) & {
        if (YAPDF_LIKELY(hasValue())) {
            return std::invoke(std::forward<F>(f), value());
        }
        return Unexpected(error());
    }

    template <typename F, typename R = std::invoke_result_t<F, T>, typename U = typename R::ValueType,
              YAPDF_REQUIRES(std::is_same_v<R, Expected<U, E>>)>
    Expected<U, E> andThen(F&& f) const& {
        if (YAPDF_LIKELY(hasValue())) {
            return std::invoke(std::forward<F>(f), value());
        }
        return Unexpected(error());
    }

    template <typename F, typename R = std::invoke_result_t<F, T>, typename U = typename R::ValueType,
              YAPDF_REQUIRES(std::is_same_v<R, Expected<U, E>>)>
    Expected<U, E> andThen(F&& f) && {
        if (YAPDF_LIKELY(hasValue())) {
            return std::invoke(std::forward<F>(f), std::move(value()));
        }
        return Unexpected(std::move(error()));
    }

    template <typename F, typename R = std::invoke_result_t<F, T>, typename U = typename R::ValueType,
              YAPDF_REQUIRES(std::is_same_v<R, Expected<U, E>>)>
    Expected<U, E> andThen(F&& f) const&& {
        if (YAPDF_LIKELY(hasValue())) {
            return std::invoke(std::forward<F>(f), std::move(value()));
        }
        return Unexpected(std::move(error()));
    }
    /// \}

    /// \{
    ///
    /// Call `f` if the `Expected` holds an `E`, otherwise return the `T`.
    ///
    /// This function can be used for control flow based on `Expected`.
    ///
    /// ``` cpp
    /// const Expected<int, int> e1 = 2;
    /// assert(e1.orElse([](int x) -> Expected<int, int> { return x * x; }).value(), 4);
    ///
    /// const Expected<int, int> e2 = Unexpected(3);
    /// assert(e2.orElse([](int x) -> Expected<int, int> { return x + 1; }).error(), 4);
    /// ```
    template <typename F, typename R = std::invoke_result_t<F, E>, typename U = typename R::ErrorType,
              YAPDF_REQUIRES(std::is_same_v<R, Expected<T, U>>)>
    Expected<T, U> orElse(F&& f) & {
        if (YAPDF_LIKELY(hasError())) {
            return std::invoke(std::forward<F>(f), error());
        }
        return value();
    }

    template <typename F, typename R = std::invoke_result_t<F, E>, typename U = typename R::ErrorType,
              YAPDF_REQUIRES(std::is_same_v<R, Expected<T, U>>)>
    Expected<T, U> orElse(F&& f) const& {
        if (YAPDF_LIKELY(hasError())) {
            return std::invoke(std::forward<F>(f), error());
        }
        return value();
    }

    template <typename F, typename R = std::invoke_result_t<F, E>, typename U = typename R::ErrorType,
              YAPDF_REQUIRES(std::is_same_v<R, Expected<T, U>>)>
    Expected<T, U> orElse(F&& f) && {
        if (YAPDF_LIKELY(hasError())) {
            return std::invoke(std::forward<F>(f), std::move(error()));
        }
        return std::move(value());
    }

    template <typename F, typename R = std::invoke_result_t<F, E>, typename U = typename R::ErrorType,
              YAPDF_REQUIRES(std::is_same_v<R, Expected<T, U>>)>
    Expected<T, U> orElse(F&& f) const&& {
        if (YAPDF_LIKELY(hasError())) {
            return std::invoke(std::forward<F>(f), std::move(error()));
        }
        return std::move(value());
    }
    /// \}

    /// \{
    ///
    /// Unwrap an `Expected` object, yielding the content of a `T`
    ///
    /// Throw `BadExpectedAccess` if `Expected` is in failed state
    T& value() & {
        if (YAPDF_UNLIKELY(hasError())) {
            throw BadExpectedAccess();
        }
        return std::get<0>(ex_);
    }

    const T& value() const& {
        if (YAPDF_UNLIKELY(hasError())) {
            throw BadExpectedAccess();
        }
        return std::get<0>(ex_);
    }

    T&& value() && {
        if (YAPDF_UNLIKELY(hasError())) {
            throw BadExpectedAccess();
        }
        return std::move(std::get<0>(ex_));
    }

    const T&& value() const&& {
        if (YAPDF_UNLIKELY(hasError())) {
            throw BadExpectedAccess();
        }
        return std::move(std::get<0>(ex_));
    }
    /// \}

    /// \{
    ///
    /// Unwrap an `Expected` object, yielding the content of an `E`
    ///
    /// Throw `BadExpectedAccess` if `Expected` is in successful state
    E& error() & {
        if (YAPDF_UNLIKELY(hasValue())) {
            throw BadExpectedAccess();
        }
        return std::get<1>(ex_);
    }

    const E& error() const& {
        if (YAPDF_UNLIKELY(hasValue())) {
            throw BadExpectedAccess();
        }
        return std::get<1>(ex_);
    }

    E&& error() && {
        if (YAPDF_UNLIKELY(hasValue())) {
            throw BadExpectedAccess();
        }
        return std::move(std::get<1>(ex_));
    }

    const E&& error() const&& {
        if (YAPDF_UNLIKELY(hasValue())) {
            throw BadExpectedAccess();
        }
        return std::move(std::get<1>(ex_));
    }
    /// \}

    /// \{
    ///
    /// Unwrap an `Expected`, yielding the content of a `T`
    ///
    /// Throw `BadExpectedAccess` with description when it's failed
    T& expect(const char* s) & {
        if (YAPDF_UNLIKELY(hasError())) {
            throw BadExpectedAccess(s);
        }
        return value();
    }

    const T& expect(const char* s) const& {
        if (YAPDF_UNLIKELY(hasError())) {
            throw BadExpectedAccess(s);
        }
        return value();
    }

    T&& expect(const char* s) && {
        if (YAPDF_UNLIKELY(hasError())) {
            throw BadExpectedAccess(s);
        }
        return std::move(value());
    }

    const T&& expect(const char* s) const&& {
        if (YAPDF_UNLIKELY(hasError())) {
            throw BadExpectedAccess(s);
        }
        return std::move(value());
    }
    /// \}

    /// \{
    ///
    /// Unwrap an `Expected`, yielding the content of an `E`
    ///
    /// Throw `BadExpectedAccess` with description when it's successful
    E& expectErr(const char* s) & {
        if (YAPDF_UNLIKELY(hasValue())) {
            throw BadExpectedAccess(s);
        }
        return error();
    }

    const E& expectErr(const char* s) const& {
        if (YAPDF_UNLIKELY(hasValue())) {
            throw BadExpectedAccess(s);
        }
        return error();
    }

    E&& expectErr(const char* s) && {
        if (YAPDF_UNLIKELY(hasValue())) {
            throw BadExpectedAccess(s);
        }
        return std::move(error());
    }

    const E&& expectErr(const char* s) const&& {
        if (YAPDF_UNLIKELY(hasValue())) {
            throw BadExpectedAccess(s);
        }
        return std::move(error());
    }
    /// \}

    /// \{
    ///
    /// Unwrap an `Expected` object, yielding the content of a `T`. Else, it returns `deft`.
    ///
    /// Arguments passed to `valueOr` are eagerly evaluated; if you're passing the result of a function call, it is
    /// recommended to use `valueOrElse`, which is lazily evaluated.
    ///
    /// ``` cpp
    /// const Expected<int, int> x(2);
    /// assert(x.valueOr(3) == 2);
    ///
    /// const Expected<int, int> y(unexpect, 2);
    /// assert(x.valueOr(3) == 3);
    /// ```
    template <typename U, YAPDF_REQUIRES(std::is_constructible_v<T, U&&>)>
    T valueOr(U&& deft) & {
        return hasValue() ? value() : static_cast<T>(std::forward<U>(deft));
    }

    template <typename U, YAPDF_REQUIRES(std::is_constructible_v<T, U&&>)>
    T valueOr(U&& deft) const& {
        return hasValue() ? value() : static_cast<T>(std::forward<U>(deft));
    }

    template <typename U, YAPDF_REQUIRES(std::is_constructible_v<T, U&&>)>
    T valueOr(U&& deft) && {
        return hasValue() ? std::move(value()) : static_cast<T>(std::forward<U>(deft));
    }

    template <typename U, YAPDF_REQUIRES(std::is_constructible_v<T, U&&>)>
    T valueOr(U&& deft) const&& {
        return hasValue() ? std::move(value()) : static_cast<T>(std::forward<U>(deft));
    }
    /// \}

    /// \{
    ///
    /// Unwrap an `Expected` object, yielding the content of a `T`. Else, it calls `f` with its failed value `E`.
    ///
    /// ``` cpp
    /// const Expected<int, int> x(2);
    /// const int resx = x.valueOrElse([](int x) { return x; });
    /// assert(resx == 2);
    ///
    /// const Expected<int, int> y(unexpect, 3);
    /// const int resy = y.valueOrElse([](int y) { return y; });
    /// assert(resy == 3);
    /// ```
    template <typename F, YAPDF_REQUIRES(std::is_same_v<std::invoke_result_t<F, E>, T>)>
    T valueOrElse(F&& f) & {
        return hasValue() ? value() : std::invoke(std::forward<F>(f), error());
    }

    template <typename F, YAPDF_REQUIRES(std::is_same_v<std::invoke_result_t<F, E>, T>)>
    T valueOrElse(F&& f) const& {
        return hasValue() ? value() : std::invoke(std::forward<F>(f), error());
    }

    template <typename F, YAPDF_REQUIRES(std::is_same_v<std::invoke_result_t<F, E>, T>)>
    T valueOrElse(F&& f) && {
        return hasValue() ? std::move(value()) : std::invoke(std::forward<F>(f), std::move(error()));
    }

    template <typename F, YAPDF_REQUIRES(std::is_same_v<std::invoke_result_t<F, E>, T>)>
    T valueOrElse(F&& f) const&& {
        return hasValue() ? std::move(value()) : std::invoke(std::forward<F>(f), std::move(error()));
    }
    /// \}

    /// Swap *this with `rhs`
    void swap(Expected& rhs) noexcept(std::is_nothrow_swappable_v<T> && std::is_nothrow_swappable_v<E>) {
        ex_.swap(rhs.ex_);
    }

private:
    std::variant<T, E> ex_;
};

/// \{
///
/// Equality operators for `Expected` and `Unexpected`
template <typename T, typename E, typename U>
bool operator==(const Expected<T, E>& lhs, const Unexpected<U>& rhs) noexcept {
    return lhs.hasError() && lhs.error() == rhs.error();
}

template <typename T, typename E, typename U>
bool operator==(const Unexpected<U>& lhs, const Expected<T, E>& rhs) noexcept {
    return rhs == lhs;
}

template <typename T, typename E, typename U>
bool operator!=(const Expected<T, E>& lhs, const Unexpected<U>& rhs) noexcept {
    return !(lhs == rhs);
}

template <typename T, typename E, typename U>
bool operator!=(const Unexpected<U>& lhs, const Expected<T, E>& rhs) noexcept {
    return !(lhs == rhs);
}
/// \}

/// \{
///
/// Equality operators for `Expected` types
template <typename T, typename E>
bool operator==(const Expected<T, E>& lhs, const Expected<T, E>& rhs) noexcept {
    return lhs.hasValue() == rhs.hasValue() ? lhs.value() == rhs.value()
                                            : lhs.hasError() == rhs.hasError() && lhs.error() == rhs.error();
}

template <typename T, typename E>
bool operator!=(const Expected<T, E>& lhs, const Expected<T, E>& rhs) noexcept {
    return !(lhs == rhs);
}
/// \}

/// Swaps two `Expected` objects
template <typename T, typename E>
void swap(Expected<T, E>& lhs, Expected<T, E>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
    return lhs.swap(rhs);
}

} // namespace yapdf

#endif // YAPDF_EXPECTED_HPP_
