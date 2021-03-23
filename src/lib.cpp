#include "bridge.hpp"
#include "pdf.hpp"

// It indicates that it's released under the GPL or compatible license.
int plugin_is_GPL_compatible;

// Emacs will call this function when it loads a dynamic module.
//
// If a module does not export a function named `emacs_module_init`, trying to load the module will signal an error. The
// initialization function should return zero if the initialization succeeds, non-zero otherwise. In the latter case,
// Emacs will signal an error, and the loading of the module will fail.
//
// If the user presses <kbd>C-g</kbd> during the initialization, Emacs ignores the return value of this initialization
// function and quits. If needed, you can catch user quitting inside the initialization function.
int emacs_module_init(struct emacs_runtime* runtime) EMACS_NOEXCEPT {
    emacs_env* env = runtime->get_environment(runtime);

    // Compatibility verification
    if (static_cast<std::size_t>(runtime->size) < sizeof(*runtime)) {
        return 1;
    }

    if (static_cast<std::size_t>(env->size) < sizeof(*env)) {
        return 2;
    }

    using namespace yapdf::emacs;

    // Create the Emacs-side function wrapper object
    Env e(env);
    const Value foo = e.make<Value::Type::Function>(
                           +[](Env&) { throw 1; }, "An example function that throws exception")
                          .expect("Create a function that throws exception");
    e.defalias("foo-fun", foo).expect("defalias foo to foo-fun");

    const Value pdf_new_fn = e.make<Value::Type::Function>(&yapdf::pdf_new, "").expect("pdf-new");
    e.defalias("pdf-new", pdf_new_fn).expect("defalias pdf-new");

    // Provide `pdf-module' to Emacs
    e.provide("pdf-module").expect("init pdf-module");

    return 0;
}

// struct SomeClass {
//     SomeClass(std::string d) : data_(std::move(d)) {}
//     ~SomeClass() { std::cout << data_ << " destroyed\n"; }
//     void DoSomething() const { std::cout << data_ << "\n"; }
// private:
//     std::string data_;
// };
//
// int emacs_module_init(struct emacs_runtime *ert) {
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
