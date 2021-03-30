#include "pdf.hpp"

#include <stdexcept>

namespace {
Gtk::Fixed* findFixedWidget(const std::vector<Gtk::Widget*>& widgets) noexcept {
    for (Gtk::Widget* w : widgets) {
        if (auto p = dynamic_cast<Gtk::Fixed*>(w)) {
            return p;
        } else if (auto p = dynamic_cast<Gtk::Box*>(w)) {
            if (Gtk::Fixed* fixed = findFixedWidget(p->get_children())) {
                return fixed;
            }
        }
    }
    return nullptr;
}

// Find Emacs widget and draw on it
Gtk::Fixed* findFocusedFixedWidget() noexcept {
    for (Gtk::Window* w : Gtk::Window::list_toplevels()) {
        if (w->has_toplevel_focus()) {
            return findFixedWidget(w->get_children());
        }
    }
    return nullptr;
}
} // namespace

namespace yapdf {
Expected<emacs::Value, emacs::Error> yapdfNew(emacs::Env& e, emacs::Value args[], std::size_t n) {
    (void)args;
    (void)n;

    auto* button = new Gtk::Button("Hello, pdf-mode");
    Gtk::Fixed* fixed = findFocusedFixedWidget();
    if (!fixed) {
        throw std::runtime_error("Emacs widget not found");
    }

    fixed->add(*button);
    fixed->show_all();
    return e.make<emacs::Value::Type::UserPtr>(button, [](void* p) EMACS_NOEXCEPT { delete (Gtk::Button*)p; });
}
YAPDF_EMACS_DEFUN(yapdfNew, 1, 1, "yapdf--new", "The yapdf--new function defined in C++");

void yapdfHide(emacs::Env&, void* p) {
    auto* button = (Gtk::Button*)p;
    button->hide();
}
YAPDF_EMACS_DEFUN(yapdfHide, "yapdf--hide", "The yapdf--hide function defined in C++");

void yapdfShow(emacs::Env&, void* p) {
    auto* button = (Gtk::Button*)p;
    button->show();
}
YAPDF_EMACS_DEFUN(yapdfShow, "yapdf--show", "The yapdf--show function defined in C++");
} // namespace yapdf
