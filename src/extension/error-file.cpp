/*
 * Authors:
 *   Ted Gould <ted@gould.cx>
 *
 * Copyright (C) 2005 Authors
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include <stdlib.h>
#include <stream.h>

// #include <glibmm/ustring.h>
#include <glibmm/i18n.h>

#include <gtkmm/checkbutton.h>
#include <gtkmm/label.h>
#include <gtkmm/stock.h>

#include <inkscape.h>
#include <prefs-utils.h>
#include <extension/extension.h>

#include "error-file.h"

/** The name and group of the preference to say whether the error
    dialog should be shown on startup. */
#define PREFERENCE_ID  "dialogs.extension-error", "show-on-startup"

namespace Inkscape {
namespace Extension {

/** \brief  An initializer which builds the dialog

    Really a simple function.  Basically the message dialog itself gets
    built with the first initializer.  The next step is to add in the
    message, and attach the filename for the error file.  After that
    the checkbox is built, and has the call back attached to it.  Also,
    it is set based on the preferences setting for show on startup (really,
    it should always be checked if you can see the dialog, but it is
    probably good to check anyway).
*/
ErrorFileNotice::ErrorFileNotice (void) :
    Gtk::MessageDialog::MessageDialog(
            "",                    /* message */
            false,                 /* use markup */
            Gtk::MESSAGE_WARNING,  /* dialog type */
            Gtk::BUTTONS_OK,       /* buttons */
            true                   /* modal */
        )

{
    /* This is some filler text, needs to change before relase */
    Glib::ustring dialog_text(_("One or more extensions failed to load.  This is probably due to you having bad karma.  Some things that could improve your karma are: walking an old lady across the street, helping out at a homeless shelter, or stop sleeping with your best friend's wife.  No, I don't care if you you think you love her.  You can find a slightly more technical description of the errors here: "));
    gchar * ext_error_file = profile_path(EXTENSION_ERROR_LOG_FILENAME);
    dialog_text += ext_error_file;
    g_free(ext_error_file);
    set_message(dialog_text, false);

    Gtk::VBox * vbox = get_vbox();

    /* This is some filler text, needs to change before relase */
    checkbutton = new Gtk::CheckButton(_("Abuse me on the next startup"));
    vbox->pack_start(*checkbutton, true, true, 5);
    checkbutton->show();
    checkbutton->set_active(prefs_get_int_attribute(PREFERENCE_ID, 1) == 0 ? false : true);

    checkbutton->signal_toggled().connect(sigc::mem_fun(this, &ErrorFileNotice::checkbox_toggle));

    return;
}

/** \brief Sets the preferences based on the checkbox value */
void
ErrorFileNotice::checkbox_toggle (void)
{
    // std::cout << "Toggle value" << std::endl;
    prefs_set_int_attribute(PREFERENCE_ID, checkbutton->get_active() ? 1 : 0);
}

/** \brief Shows the dialog

    This function only shows the dialog if the preferences say that the
    user wants to see the dialog, otherwise it just exits.
*/
int
ErrorFileNotice::run (void)
{
    if (prefs_get_int_attribute(PREFERENCE_ID, 1) == 0)
        return 0;
    return Gtk::Dialog::run();
}

}; };  /* namespace Inkscape, Extension */

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
