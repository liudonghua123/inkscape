/**
 * \brief Labelled Widget - Adds a label with optional icon or suffix to
 *        another widget.
 *
 * Authors:
 *   Carl Hetherington <inkscape@carlh.net>
 *   Derek P. Moore <derekm@hackunix.org>
 *
 * Copyright (C) 2004 Carl Hetherington
 *
 * Released under GNU GPL.  Read the file 'COPYING' for more information.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "labelled.h"

namespace Inkscape {
namespace UI {
namespace Widget {

/**
 * Construct a Labelled Widget.
 *
 * \param label     Label.
 * \param widget    Widget to label; should be allocated with new, as it will
 *                  be passed to Gtk::manage().
 * \param suffix    Suffix, placed after the widget (defaults to "").
 * \param icon      Icon filename, placed before the label (defaults to "").
 * \param mnemonic  Mnemonic toggle; if true, an underscore (_) in the text
 *                  indicates the next character should be used for the
 *                  mnemonic accelerator key (defaults to false).
 */
Labelled::Labelled(Glib::ustring const &label,
                   Gtk::Widget *widget,
                   Glib::ustring const &suffix,
                   Glib::ustring const &icon,
                   bool mnemonic)
    : _widget(widget),
      _label(new Gtk::Label(label, 0.0, 0.5, mnemonic)),
      _suffix(new Gtk::Label(suffix, 0.0, 0.5)),
      _icon(new Gtk::Image(icon))
{
    if (icon != "") {
        pack_start(*Gtk::manage(_icon), Gtk::PACK_SHRINK);
    }
    pack_start(*Gtk::manage(_label), Gtk::PACK_EXPAND_WIDGET, 6);
    pack_start(*Gtk::manage(_widget), Gtk::PACK_SHRINK, 6);
    if (mnemonic) {
        _label->set_mnemonic_widget(*_widget);
    }
}

} // namespace Widget
} // namespace UI
} // namespace Inkscape

/* 
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=c++:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
