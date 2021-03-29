//! Poppler PDF
//!
//! SPDX-License-Identifier: GPL-2.0-or-later

#ifndef YAPDF_PDF_HPP_
#define YAPDF_PDF_HPP_

#include "bridge.hpp"
#include <chrono>
#include <thread>

#include <gtkmm.h>

namespace yapdf {
class MyWindow : public Gtk::Window {
public:
    MyWindow() {
        set_title("Basic application");
        set_default_size(200, 200);
    }
};

static Gtk::Fixed* find_fixed_widget(const std::vector<Gtk::Widget*>& widgets) {
    for (Gtk::Widget* w : widgets) {
        if (auto p = dynamic_cast<Gtk::Fixed*>(w)) {
            return p;
        } else if (auto p = dynamic_cast<Gtk::Box*>(w)) {
            if (Gtk::Fixed* fixed = find_fixed_widget(p->get_children())) {
                return fixed;
            }
        }
    }
    return nullptr;
}

static Gtk::Fixed* find_focused_fixed_widget() {
    for (Gtk::Window* w : Gtk::Window::list_toplevels()) {
        if (w->has_toplevel_focus()) {
            return find_fixed_widget(w->get_children());
        }
    }
    return nullptr;
}

static Expected<emacs::Value, emacs::Error> yapdf_new(emacs::Env& e, emacs::Value args[], std::size_t n) {
    auto* button = new Gtk::Button("Hello, pdf-mode");
    Gtk::Fixed* fixed = find_focused_fixed_widget();
    if (!fixed) {
        __builtin_printf("not found\n");
    }

    fixed->add(*button);
    fixed->show_all();

    return e.make<emacs::Value::Type::UserPtr>(button, [](void* p) EMACS_NOEXCEPT { delete (Gtk::Button*)p; });
}

static void yapdf_hide(emacs::Env&, void* p) {
    auto* button = (Gtk::Button*)p;
    button->hide();
}

} // namespace yapdf

#endif // YAPDF_PDF_HPP_
