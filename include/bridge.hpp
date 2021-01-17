//! Thin bridge between Emacs and C++.
//! SPDX-License-Identifier: GPL-2.0-or-later

#ifndef YAPDF_BRIDGE_HPP_
#define YAPDF_BRIDGE_HPP_

#include <cstdint>
#include <cstring>
#include <string>

#include <emacs-module.h>

#define YAPDF_EMACS_APPLY(env, f, ...) (env)->f((env), __VA_ARGS__)

namespace yapdf {
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

        /// Return the type of a Lisp symbol. It corresponds exactly to the
        /// `type-of` Lisp function.
        Type typeOf() {
            const Value t(emacs_,
                          YAPDF_EMACS_APPLY(emacs_->env_, type_of, value_));
            if (t == emacs_->intern("integer")) {
                return Type::Integer;
            } else if (t == emacs_->intern("symbol")) {
                return Type::Symbol;
            } else if (t == emacs_->intern("string")) {
                return Type::String;
            } else if (t == emacs_->intern("cons")) {
                return Type::Cons;
            } else if (t == emacs_->intern("float")) {
                return Type::Float;
            }
            return Type::Unknown;
        }

        /// Return the integral value stored in the Emacs integer object.
        ///
        /// If it doesn't represent an integer object, Emacs will signal an
        /// error of type `wrong-type-argument`. If the integer represented by
        /// `Value` can't be represented as `std::intmax_t`, Emacs will signal
        /// an error of type `overflow-error`.
        std::intmax_t asInteger() {
            return YAPDF_EMACS_APPLY(emacs_->env_, extract_integer, value_);
        }

        /// Return the value stored in the Emacs floating-point number.
        ///
        /// If it doesn't represent a floating-point object, Emacs will signal
        /// an error of type `wrong-type-argument`.
        double asFloat() {
            return YAPDF_EMACS_APPLY(emacs_->env_, extract_float, value_);
        }

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
        emacs_value native() {
            return value_;
        }

        /// Check whether the Lisp object is not `nil`.
        ///
        /// It never exists non-locally. There can be multiple different values
        /// that represent `nil`.
        operator bool() {
            return YAPDF_EMACS_APPLY(emacs_->env_, is_not_nil, value_);
        }

        /// Check whether `*this` and `rhs` represent the same Lisp object.
        ///
        /// It never exists non-locally. `operator==` corresponds to the Lisp
        /// `eq` function. For other kinds of equality comparisons, such as
        /// `eql`, `equal`, use `intern` and `funcall` to call the corresponding
        /// Lisp function.
        ///
        /// # Note
        ///
        /// Two `Value` objects that are different in the C sense might still
        /// represent the same Lisp object, so you must always call `operator==`
        /// to check for equality.
        bool operator==(const Value& rhs) const {
            return YAPDF_EMACS_APPLY(emacs_->env_, eq, value_, rhs.value_);
        }

        bool operator!=(const Value& rhs) const {
            return !(*this == rhs);
        }

    private:
        Value(Emacs* emacs, emacs_value value) : emacs_(emacs), value_(value) {}

        Emacs* emacs_;
        emacs_value value_;
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

    /// Possible return values for ProcessInput
    enum class ProcessInputResult {
        /// Module code may continue
        Continue = 0,
        /// Module code should return control to Emacs as soon as possible
        Quit = 1,
    };

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
    /// If the value can't be represented as an Emacs integer, Emacs will signal
    /// an error of type `overflow-error`.
    Value makeInteger(std::intmax_t v) {
        return Value(this, YAPDF_EMACS_APPLY(env_, make_integer, v));
    }

    /// Create an Emacs floating-point number from a C floating-point value.
    Value makeFloat(double v) {
        return Value(this, YAPDF_EMACS_APPLY(env_, make_float, v));
    }

    /// Create a multibyte Lisp string object.
    ///
    /// If `len` is larger than the maximum allowed Emacs string length, Emacs
    /// will raise an `overflow-error` signal. Otherwise, Emacs treats the
    /// memory at `s` as the UTF-8 representation of a string.
    ///
    /// If the memory block delimited by `s` and `len` contains a valid UTF-8
    /// string, the result value will be a multibyte Lisp string that contains
    /// the same sequence of Unicode scalar values as represented by `s`.
    /// Otherwise, the return value will be a multibyte Lisp string with
    /// unspecified contents; in practice, Emacs will attempt to detect as many
    /// valid UTF-8 subsequence in `s` as possible and treat the rest as
    /// undecodable bytes, but you shouldn't rely on any specific behavior in
    /// this case.
    ///
    /// The returned Lisp string will not contain any text properties. To create
    /// a string containing text properties, use `funcall` to call functions
    /// such as `propertize`.
    ///
    /// `makeString` can't create strings that contains characters that are not
    /// valid Unicode scalar values. Such strings are rare, but occur from time
    /// to time; examples are strings with UTF-16 surrogate code points or
    /// strings with extended Emacs characters that don't correspond to Unicode
    /// code points. To create such a Lisp string, call e.g. the function
    /// `string` and pass the desired character values as integers.
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

    // function register
    void make_function();
    void funcall();

    /// Return the canonical symbol whose name is `s`.
    Value intern(const char* s) {
        const emacs_value val = YAPDF_EMACS_APPLY(env_, intern, s);
        return Value(this, val);
    }

    /// Get the native implementation `emacs_env*`
    emacs_env* native() {
        return env_;
    }

#if EMACS_MAJOR_VERSION >= 26
    bool should_quit();
#endif

#if EMACS_MAJOR_VERSION >= 27
// enum emacs_process_input_result (*process_input) (emacs_env *env)
//     EMACS_ATTRIBUTE_NONNULL (1);

//   struct timespec (*extract_time) (emacs_env *env, emacs_value arg)
//     EMACS_ATTRIBUTE_NONNULL (1);

//   emacs_value (*make_time) (emacs_env *env, struct timespec time)
//     EMACS_ATTRIBUTE_NONNULL (1);

//   bool (*extract_big_integer) (emacs_env *env, emacs_value arg, int *sign,
//                                ptrdiff_t *count, emacs_limb_t *magnitude)
//     EMACS_ATTRIBUTE_NONNULL (1);

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
