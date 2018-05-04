/*
 * Authors:
 *   Theodore Janeczko
 *
 * Copyright (C) Theodore Janeczko 2012 <flutterguy317@gmail.com>
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ui/widget/clipmaskicon.h"

#include <gtkmm/icontheme.h>

#include "widgets/toolbox.h"
#include "ui/icon-names.h"
#include "layertypeicon.h"

namespace Inkscape {
namespace UI {
namespace Widget {

ClipMaskIcon::ClipMaskIcon() :
    Glib::ObjectBase(typeid(ClipMaskIcon)),
    Gtk::CellRendererPixbuf(),
    _pixClipName(INKSCAPE_ICON("path-cut")),
    _pixMaskName(INKSCAPE_ICON("path-difference")),
    _pixBothName(INKSCAPE_ICON("bitmap-trace")),
    _property_active(*this, "active", 0),
    _property_pixbuf_clip(*this, "pixbuf_on", Glib::RefPtr<Gdk::Pixbuf>(0)),
    _property_pixbuf_mask(*this, "pixbuf_off", Glib::RefPtr<Gdk::Pixbuf>(0)),
    _property_pixbuf_both(*this, "pixbuf_on", Glib::RefPtr<Gdk::Pixbuf>(0))
{
    
    property_mode() = Gtk::CELL_RENDERER_MODE_ACTIVATABLE;

    gint width, height;
    gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &width, &height);
    phys = width;

    Glib::RefPtr<Gtk::IconTheme> icon_theme = Gtk::IconTheme::get_default();
    _property_pixbuf_clip = icon_theme->load_icon(_pixClipName, phys, (Gtk::IconLookupFlags)0);
    _property_pixbuf_mask = icon_theme->load_icon(_pixMaskName, phys, (Gtk::IconLookupFlags)0);
    _property_pixbuf_both = icon_theme->load_icon(_pixBothName, phys, (Gtk::IconLookupFlags)0);

    property_pixbuf() = Glib::RefPtr<Gdk::Pixbuf>(0);
}

void ClipMaskIcon::get_preferred_height_vfunc(Gtk::Widget& widget,
                                              int& min_h,
                                              int& nat_h) const
{
    Gtk::CellRendererPixbuf::get_preferred_height_vfunc(widget, min_h, nat_h);

    if (min_h) {
        min_h += (min_h) >> 1;
    }
    
    if (nat_h) {
        nat_h += (nat_h) >> 1;
    }
}

void ClipMaskIcon::get_preferred_width_vfunc(Gtk::Widget& widget,
                                             int& min_w,
                                             int& nat_w) const
{
    Gtk::CellRendererPixbuf::get_preferred_width_vfunc(widget, min_w, nat_w);

    if (min_w) {
        min_w += (min_w) >> 1;
    }
    
    if (nat_w) {
        nat_w += (nat_w) >> 1;
    }
}

void ClipMaskIcon::render_vfunc( const Cairo::RefPtr<Cairo::Context>& cr,
                                 Gtk::Widget& widget,
                                 const Gdk::Rectangle& background_area,
                                 const Gdk::Rectangle& cell_area,
                                 Gtk::CellRendererState flags )
{
    switch (_property_active.get_value())
    {
        case 1:
            property_pixbuf() = _property_pixbuf_clip;
            break;
        case 2:
            property_pixbuf() = _property_pixbuf_mask;
            break;
        case 3:
            property_pixbuf() = _property_pixbuf_both;
            break;
        default:
            property_pixbuf() = Glib::RefPtr<Gdk::Pixbuf>(0);
            break;
    }
    Gtk::CellRendererPixbuf::render_vfunc( cr, widget, background_area, cell_area, flags );
}

bool ClipMaskIcon::activate_vfunc(GdkEvent* /*event*/,
                                  Gtk::Widget& /*widget*/,
                                  const Glib::ustring& /*path*/,
                                  const Gdk::Rectangle& /*background_area*/,
                                  const Gdk::Rectangle& /*cell_area*/,
                                  Gtk::CellRendererState /*flags*/)
{
    return false;
}


} // namespace Widget
} // namespace UI
} // namespace Inkscape

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :


