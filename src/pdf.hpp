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

void pdf_new(emacs::Env&) {
    auto w = new Gtk::Window;
    w->set_default_size(200, 200);

    w->show_all();
}
} // namespace yapdf

#endif // YAPDF_PDF_HPP_
