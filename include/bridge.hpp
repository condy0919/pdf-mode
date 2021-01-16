//! Thin bridge between Emacs and C++.
//! SPDX-License-Identifier: GPL-2.0-or-later

#ifndef YAPDF_BRIDGE_HPP_
#define YAPDF_BRIDGE_HPP_

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
        void type_of();
        // void eq();
        void extract_integer();
        void make_integer();
        void extract_float();
        void make_float();

        void copy_string_contents();
        void make_string();

        void make_user_ptr();
        void get_user_ptr();
        void set_user_ptr();

        void get_user_finalizer();
        void set_user_finalizer();

        void vec_get();
        void vec_set();
        void vec_size();

        /// Check whether the Lisp object is not `nil`.
        ///
        /// It never exists non-locally. There can be multiple different values
        /// that represent `nil`.
        operator bool() EMACS_NOEXCEPT {
            return YAPDF_EMACS_APPLY(emacs_->env_, is_not_nil, value_);
        }

        /// Check whether
        bool operator==(const Value& rhs) EMACS_NOEXCEPT {
            return YAPDF_EMACS_APPLY(emacs_->env_, eq, value_, rhs.value_);
        }

        bool operator!=(const Value& rhs) EMACS_NOEXCEPT {
            return !(*this == rhs);
        }

    private:
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
    Value makeGlobalRef(Value value) EMACS_NOEXCEPT;
    void freeGlobalRef(Value value) EMACS_NOEXCEPT;

    // non-local exit handling
    void non_local_exit_check();
    void non_local_exit_clear();
    void non_local_exit_get();
    void non_local_exit_signal();
    void non_local_exit_throw();

    // function register
    void make_function();
    void funcall();
    void intern();

    /// Get the native `emacs_env*`
    emacs_env* native() EMACS_NOEXCEPT {
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
