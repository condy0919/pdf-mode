#include <cassert>
#include <cstddef>
#include <emacs-module.h>

// `plugin_is_GPL_compatible' indicates that it's released under the GPL or
// compatible license.
int plugin_is_GPL_compatible;

int emacs_module_init(struct emacs_runtime* ert) {
    emacs_env* env = ert->get_environment(ert);

    // register a simple function
    // create the Emacs-side function wrapper object
    emacs_value fn = env->make_function(
        env, 1, 1,
        [](emacs_env* env, std::ptrdiff_t, emacs_value args[], void*)
            EMACS_NOEXCEPT {
                const long result = env->extract_integer(env, args[0]) * 2;
                return env->make_integer(env, result);
            },
        "An example function that doubles its argument", nullptr);

    emacs_value args[] = {env->make_integer(env, 3)};
    emacs_value retval = env->funcall(env, fn, 1, args);
    assert(env->extract_integer(env, retval) == 6);

    emacs_value fname = env->intern(env, "yapdf-unary-int-fn");
    emacs_value fset = env->intern(env, "fset");
    emacs_value args2[] = {fname, fn};
    env->funcall(env, fset, 2, args2);

    return 0;
}

// // Very rudimentary binding example, for now.
// // You can test all of this with a one-liner:
// // emacs -l ./libdymod.so -f dymod-sample-nullary-void-fn --eval='(message
// "computed %d" (dymod-sample-unary-int-fn 8))'
// // it will print "Hello Elisp" twice on the console
// // "Foo" and "Bar" also (for the class instances)
// // and "computed 16" in the echo area
//
// // Our API
// void nullary_void_fn() { std::cout << "Hello Elisp\n"; }
// int  unary_int_fn(int i) { return 2*i; }
//
// struct SomeClass {
//     SomeClass(std::string d) : data_(std::move(d)) {}
//     ~SomeClass() { std::cout << data_ << " destroyed\n"; }
//     void DoSomething() const { std::cout << data_ << "\n"; }
// private:
//     std::string data_;
// };
//
// int emacs_module_init(struct emacs_runtime *ert) {
//
//     // now a member function
//     // This is similar to the other functions but we need to get the "this"
//     pointer
//     // back so we know which object it applies to
//
//     auto foo = new SomeClass("Foo");
//     auto foo_value = eenv->make_user_ptr(eenv,
//                                          [](void *p){ delete
//                                          static_cast<SomeClass*>(p); }, foo);
//     auto bar = new SomeClass("Bar");
//     /* auto bar_value = */ eenv->make_user_ptr(eenv,
//                                          [](void *p){ delete
//                                          static_cast<SomeClass*>(p); }, bar);
//
//     // a function to execute the DoSomething member of a SomeClass
//     auto doer = [](emacs_env *env, ptrdiff_t, emacs_value [] , void* that) {
//         static_cast<SomeClass*>(that)->DoSomething();
//         return env->intern(env, "nil");
//     };
//
//     // now bind it to the two different instances of SomeClass
//     emacs_value foo_fn = eenv->make_function(eenv, 0, 0, doer,
//                                              "calls foo's DoSomething
//                                              method", foo);
//     emacs_value bar_fn = eenv->make_function(eenv, 0, 0, doer,
//                                              "calls bar's DoSomething
//                                              method", bar);
//
//     // and call them
//     eenv->funcall(eenv, foo_fn, 0, nullptr);
//     eenv->funcall(eenv, bar_fn, 0, nullptr);
//
//     // To demonstrate finalizers, let's bind one of our SomeClass instances
//     // to a variable name - but not the other
//     emacs_value foo_symbol = eenv->intern(eenv, "dymod-sample-foo");
//
//     // we will need the set function
//     emacs_value set = eenv->intern(eenv, "set");
//
//     // perform the set
//     emacs_value args_for_set[]{foo_symbol, foo_value};
//     eenv->funcall(eenv, set, 2, args_for_set);
//
//     // now when garbage collection is performed only bar will be collected
//     // (and its destructor message "Bar destroyed" printed)
//
//     return 0;
// }
