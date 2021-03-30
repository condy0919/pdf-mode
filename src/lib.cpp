#include "bridge.hpp"
#include "pdf.hpp"

#include <gtkmm.h>

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

    // Initialize GTK
    Gtk::Main::init_gtkmm_internals();

    // Initialize yapdf
    yapdf::emacs::Env e(env);
    yapdf::emacs::DefunRegistry::getInstance().def(e);

    // Provide `pdf-module' to Emacs
    e.provide("yapdf-module").expect("init yapdf-module");

    return 0;
}
