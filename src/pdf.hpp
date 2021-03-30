//! Poppler PDF
//!
//! SPDX-License-Identifier: GPL-2.0-or-later

#ifndef YAPDF_PDF_HPP_
#define YAPDF_PDF_HPP_

#include <gtkmm.h>

#include "bridge.hpp"

namespace yapdf {
/// A Simple demo
///
/// Currently unused
class SimplePDFViewer {
public:
    SimplePDFViewer() : btn_("Hello, pdf-mode") {}

private:
    Gtk::Button btn_;
};

} // namespace yapdf

#endif // YAPDF_PDF_HPP_
