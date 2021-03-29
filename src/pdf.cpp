#include "pdf.hpp"
#include "bridge.hpp"

namespace yapdf {

void foo(emacs::Env&) {
    throw 1;
}
YAPDF_EMACS_DEFUN(foo, "foo-fun", "An example function that throws expcetion");
} // namespace yapdf

// void yapdf_show(yapdf::emacs::Env&, void* p) {
//     auto* button = (Gtk::Button*)p;
//     button->show();
// }
// static ::yapdf::emacs::Defun* foo [[maybe_unused]] = ::yapdf::emacs::DefunRegistry::registra(
//     ::yapdf::emacs::defsubr(yapdf_show, "yapdf--show", "The yapdf--show function"));
