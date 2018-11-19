// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * The main Inkscape application.
 *
 * Copyright (C) 2018 Tavmjong Bah
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 *
 */

#include <iostream>

#include <glibmm/i18n.h>  // Internationalization

#include "inkscape-application.h"

#include "inkscape.h"             // Inkscape::Application
#include "inkgc/gc-core.h"        // Garbage Collecting init
#include "ui/widget/panel.h"      // Panel prep
#include "file.h"                 // File open and window creation.
#include "io/file.h"              // File open (command line).
#include "desktop.h"              // Access to window
#include "actions/actions-base.h" // Actions

#ifdef WITH_DBUS
# include "extension/dbus/dbus-init.h"
#endif

#ifdef ENABLE_NLS
// Native Language Support - shouldn't this always be used?
#include "helper/gettext.h"   // gettext init
#endif // ENABLE_NLS

#include "io/resource.h"
using Inkscape::IO::Resource::UIS;

// This is a bit confusing as there are two ways to handle command line arguments and files
// depending on if the Gio::APPLICATION_HANDLES_OPEN and/or Gio::APPLICATION_HANDLES_COMMAND_LINE
// flags are set. If the open flag is set and the command line not, the all the remainng arguments
// after calling on_handle_local_options() are assumed to be filenames.

InkscapeApplication::InkscapeApplication()
    : Gtk::Application("org.inkscape.application.with_gui",
                       Gio::APPLICATION_HANDLES_OPEN | // Use default file opening.
                       Gio::APPLICATION_NON_UNIQUE   ) // Allows different instances of Inkscape to run at same time.
    , _with_gui(true)
{

    // ==================== Initializations =====================
    // Garbage Collector
    Inkscape::GC::init();

#ifdef ENABLE_NLS
    // Native Language Support (shouldn't this always be used?).
    Inkscape::initialize_gettext();
#endif

    Glib::set_application_name(N_("Inkscape - A Vector Drawing Program"));  // After gettext() init.

    // ======================== Actions =========================
    add_actions_base(this);  // actions that are GUI independent

    // ====================== Command Line ======================

    // Will automatically handle character conversions.
    // Note: OPTION_TYPE_FILENAME => std::string, OPTION_TYPE_STRING => Glib::ustring.

    // Actions
    add_main_option_entry(OPTION_TYPE_STRING,   "actions",             'a', N_("Actions (with optional arguments), semi-colon separated."),     N_("ACTION(:ARGUMENT)"));
    add_main_option_entry(OPTION_TYPE_BOOL,     "action-list",        '\0', N_("Actions: List available actions."),                                                  "");

    // Query
    add_main_option_entry(OPTION_TYPE_BOOL,     "version",             'v', N_("Print: Inkscape version."),                                                          "");
    add_main_option_entry(OPTION_TYPE_BOOL,     "extensions-directory",'x', N_("Print: Extensions directory."),                                                      "");
    add_main_option_entry(OPTION_TYPE_BOOL,     "verb-list",          '\0', N_("Print: List verbs."),                                                                "");

    // Interface
    add_main_option_entry(OPTION_TYPE_BOOL,     "with-gui",            'g', N_("GUI: With graphical interface."),                                                    "");
    add_main_option_entry(OPTION_TYPE_BOOL,     "without-gui",         'G', N_("GUI: Console only."),                                                                "");

    // Open/Import
    add_main_option_entry(OPTION_TYPE_INT,      "pdf-page",           '\0', N_("Open: PDF page to import"),         N_("PAGE"));
    add_main_option_entry(OPTION_TYPE_STRING,   "convert-dpi-method", '\0', N_("Open: Method used to convert pre-0.92 document dpi, if needed: [none|scale-viewbox|scale-document]."), "[...]");
    add_main_option_entry(OPTION_TYPE_BOOL,     "no-convert-text-baseline-spacing", 0, N_("Open: Do not fix pre-0.92 document's text baseline spacing on opening."), "");

    // Query - Geometry
    add_main_option_entry(OPTION_TYPE_STRING,   "query-id",            'I', N_("Query: ID of object to be queried."),                                          N_("ID"));
    add_main_option_entry(OPTION_TYPE_BOOL,     "query-all",           'S', N_("Query: Print bounding boxes of all objects."),                                       "");
    add_main_option_entry(OPTION_TYPE_BOOL,     "query-x",             'X', N_("Query: X coordinate of drawing or object (if specified by --query-id)."),            "");
    add_main_option_entry(OPTION_TYPE_BOOL,     "query-y",             'Y', N_("Query: Y coordinate of drawing or object (if specified by --query-id)."),            "");
    add_main_option_entry(OPTION_TYPE_BOOL,     "query-width",         'W', N_("Query: Width of drawing or object (if specified by --query-id)."),                   "");
    add_main_option_entry(OPTION_TYPE_BOOL,     "query-height",        'H', N_("Query: Heightof drawing or object (if specified by --query-id)."),                   "");

    // Processing
    add_main_option_entry(OPTION_TYPE_BOOL,     "vacuum-defs",        '\0', N_("Process: Remove unused definitions from the <defs> section(s) of document."),        "");
    add_main_option_entry(OPTION_TYPE_STRING,   "select",             '\0', N_("Process: Select objects: comma separated list of IDs."),   N_("OBJECT-ID[,OBJECT-ID]*"));
    add_main_option_entry(OPTION_TYPE_STRING,   "verb",               '\0', N_("Process: Verb(s) to call when Inkscape opens."),               N_("VERB-ID[,VERB-ID]*"));
  //add_main_option_entry(OPTION_TYPE_BOOL,     "shell",              '\0', N_("Process: Start Inkscape in interative shell mode."),                                 "");

    // Export - File and File Type
    add_main_option_entry(OPTION_TYPE_STRING,   "export-type",        '\0', N_("Export: File type:[svg,png,ps,psf,tex,emf,wmf,xaml]"),                          "[...]");
    add_main_option_entry(OPTION_TYPE_FILENAME, "export-file",         'o', N_("Export: File name"),                                              N_("EXPORT-FILENAME"));
    add_main_option_entry(OPTION_TYPE_BOOL,     "export-overwrite",   '\0', N_("Export: Overwrite input file."),                                                     ""); // BSP

    //                                                                                                                                          B = PNG, S = SVG, P = PS/EPS/PDF
    // Export - Geometry
    add_main_option_entry(OPTION_TYPE_STRING,   "export-area",         'a', N_("Export: Area to export in SVG user units."),                          N_("x0:y0:x1:y1")); // BSP
    add_main_option_entry(OPTION_TYPE_BOOL,     "export-area-drawing", 'D', N_("Export: Area to export is drawing (not page)."),                                     ""); // BSP
    add_main_option_entry(OPTION_TYPE_BOOL,     "export-area-page",    'C', N_("Export: Area to export is page."),                                                   ""); // BSP
    add_main_option_entry(OPTION_TYPE_INT,      "export-margin",      '\0', N_("Export: Margin around export area: units of page size for SVG, mm for PS/EPS/PDF."), ""); // xSP
    add_main_option_entry(OPTION_TYPE_BOOL,     "export-area-snap",   '\0', N_("Export: Snap the bitmap export area outwards to the nearest integer values."),       ""); // Bxx
    add_main_option_entry(OPTION_TYPE_INT,      "export-width",        'w', N_("Export: Bitmap width in pixels (overrides --export-dpi)."),                 N_("WIDTH")); // Bxx
    add_main_option_entry(OPTION_TYPE_INT,      "export-height",       'h', N_("Export: Bitmap height in pixels (overrides --export-dpi)."),               N_("HEIGHT")); // Bxx

    // Export - Options
    add_main_option_entry(OPTION_TYPE_STRING,   "export-id",           'i', N_("Export: ID of object to export."),                                      N_("OBJECT-ID")); // BSP
    add_main_option_entry(OPTION_TYPE_BOOL,     "export-id-only",      'j', N_("Export: Hide all objects except object with ID selected by export-id."),             ""); // BSx
    add_main_option_entry(OPTION_TYPE_BOOL,     "export-plain-svg",    'l', N_("Export: Remove items in the Inkscape namespace."),                                   ""); // xSx
    add_main_option_entry(OPTION_TYPE_INT,      "export-dpi",          'd', N_("Export: Resolution for rasterization bitmaps and filters (default is 96)."),  N_("DPI")); // BxP
    add_main_option_entry(OPTION_TYPE_BOOL,     "export-ignore-filters", '\0', N_("Export: Render objects without filters instead of rasterizing. (PS/EPS/PDF)"),    ""); // xxP
    add_main_option_entry(OPTION_TYPE_BOOL,     "export-text-to-path", 'T', N_("Export: Convert text to paths. (PS/EPS/PDF/SVG)."),                                  ""); // xxP
    add_main_option_entry(OPTION_TYPE_INT,      "export-ps-level",    '\0', N_("Export: Postscript level (2 or 3). Default is 3."),                      N_("PS-Level")); // xxP
    add_main_option_entry(OPTION_TYPE_STRING,   "export-pdf-level",   '\0', N_("Export: PDF level (1.4 or 1.5)"),                                       N_("PDF-Level")); // xxP
    add_main_option_entry(OPTION_TYPE_BOOL,     "export-latex",       '\0', N_("Export: Export text separately to LaTeX file (PS/EPS/PDF). Include via \\input{file.tex}"), ""); // xxP
    add_main_option_entry(OPTION_TYPE_BOOL,     "export-use-hints",    't', N_("Export: Use stored filename and DPI hints when exporting object selected by --export-id."), ""); // Bxx
    add_main_option_entry(OPTION_TYPE_STRING,   "export-background",   'b', N_("Export: Background color for exported bitmaps (any SVG color string)."),    N_("COLOR")); // Bxx
    add_main_option_entry(OPTION_TYPE_DOUBLE,   "export-background-opacity", 'y', N_("Export: Background opacity for exported bitmaps (either 0.0 to 1.0 or 1 to 255)."), N_("VALUE")); // Bxx

#ifdef WITH_YAML
    add_main_option_entry(OPTION_TYPE_FILENAME, "xverbs",             '\0', N_("Process: xverb command file."),                                   N_("XVERBS-FILENAME"));
#endif // WITH_YAML

#ifdef WITH_DBUS
    add_main_option_entry(OPTION_TYPE_BOOL,     "dbus-listen",        '\0', N_("D-Bus: Enter a listening loop for D-Bus messages in console mode."),                 "");
    add_main_option_entry(OPTION_TYPE_STRING,   "dbus-name",          '\0', N_("D-Bus: Specify the D-Bus name (default is 'org.inkscape')."),            N_("BUS-NAME"));
#endif // WITH_DBUS
    
    signal_handle_local_options().connect(sigc::mem_fun(*this, &InkscapeApplication::on_handle_local_options));

    // This is normally called for us... but after the "handle_local_options" signal is emitted. If
    // we want to rely on actions for handling options, we need to call it here. This appears to
    // have no unwanted side-effect. It will also trigger the call to on_startup().
    register_application();
}

Glib::RefPtr<InkscapeApplication> InkscapeApplication::create()
{
    return Glib::RefPtr<InkscapeApplication>(new InkscapeApplication());
}

SPDocument*
InkscapeApplication::get_active_document()
{
    // This should change based on last document window in focus if with GUI.  But for now we're
    // only using it for command line mode so return last document (the one currently be read in).
    return _documents.back();
}

void
InkscapeApplication::on_startup()
{
    Gtk::Application::on_startup();
}

// Here are things that should be in on_startup() but cannot be as we don't set _with_gui until
// on_handle_local_options() is called.
void
InkscapeApplication::on_startup2()
{
    // This should be completely rewritten.
    Inkscape::Application::create(nullptr, _with_gui); // argv appears to not be used.

    if (!_with_gui) {
        return;
    }

    // ======================= Actions (GUI) ======================
    add_action("new",    sigc::mem_fun(*this, &InkscapeApplication::on_new   ));
    add_action("quit",   sigc::mem_fun(*this, &InkscapeApplication::on_quit  ));

    // ========================= GUI Init =========================
    Gtk::Window::set_default_icon_name("inkscape");
    Inkscape::UI::Widget::Panel::prep();

    // ========================= Builder ==========================
    _builder = Gtk::Builder::create();

    Glib::ustring app_builder_file = get_filename(UIS, "inkscape-application.xml");

    try
    {
        _builder->add_from_file(app_builder_file);
    }
    catch (const Glib::Error& ex)
    {
        std::cerr << "InkscapeApplication: " << app_builder_file << " file not read! " << ex.what() << std::endl;
    }

    auto object = _builder->get_object("menu-application");
    auto menu = Glib::RefPtr<Gio::Menu>::cast_dynamic(object);
    if (!menu) {
        std::cerr << "InkscapeApplication: failed to load application menu!" << std::endl;
    } else {
        set_app_menu(menu);
    }
}

// Open document window with default document. Either this or on_open() is called.
void
InkscapeApplication::on_activate()
{
    on_startup2();

    if (_with_gui) {
        create_window();
    } else {
        std::cerr << "InkscapeApplication::on_activate:  Without GUI" << std::endl;
        // Create blank document?
    }
}

// Open document window for each file. Either this or on_activate() is called.
// type_vec_files == std::vector<Glib::RefPtr<Gio::File> >
void
InkscapeApplication::on_open(const Gio::Application::type_vec_files& files, const Glib::ustring& hint)
{
    on_startup2();

    for (auto file : files) {
        if (_with_gui) {
            // Create a window for each file.

            create_window(file);

            // Process each file.
            for (auto action: _command_line_actions) {
                activate_action( action.first, action.second );
            }

        } else {

            // Open file
            SPDocument *doc = ink_file_open(file);
            if (!doc) continue;

            // Add to Inkscape::Application...
            INKSCAPE.add_document(doc);

            doc->ensureUpToDate(); // Or queries don't work!

            // Add to our application
            _documents.push_back(doc);

            // process_file(file);
            for (auto action: _command_line_actions) {
                activate_action( action.first, action.second );
            }

            // Save... can't use action yet.
            _file_export.do_export(doc, file->get_path());

            // Remove from our application... we only have one in command-line mode.
            _documents.pop_back();

            // Close file
            INKSCAPE.remove_document(doc);
            delete doc;
        }
    }

    //Call the base class's implementation:
    // Gtk::Application::on_open(files, hint);
}

void
InkscapeApplication::create_window(const Glib::RefPtr<Gio::File>& file)
{
    SPDesktop* desktop = nullptr;
    if (file) {
        desktop = sp_file_new_default();
        sp_file_open(file->get_parse_name(), nullptr, false, true);
    } else {
        desktop = sp_file_new_default();
    }

    _documents.push_back(desktop->getDocument());

    // Add to Gtk::Window to app window list.
    add_window(*desktop->getToplevel());
}

// ========================= Callbacks ==========================

/*
 * Handle command line options.
 *
 * Options are processed in the order they appear in this function.
 * We process in order: Print -> GUI -> Open -> Query -> Process -> Export.
 * For each file without GUI: Open -> Query -> Process -> Export 
 * More flexible processing can be done via actions or xverbs.
 */
int
InkscapeApplication::on_handle_local_options(const Glib::RefPtr<Glib::VariantDict>& options)
{
    if (!options) {
        std::cerr << "InkscapeApplication::on_handle_local_options: options is null!" << std::endl;
        return -1; // Keep going
    }

    // ===================== QUERY =====================
    // These are processed first as they result in immediate program termination.
    if (options->contains("version")) {
        activate_action("inkscape-version");
        return EXIT_SUCCESS;
    }

    if (options->contains("extensions-directory")) {
        activate_action("extensions-directory");
        return EXIT_SUCCESS;
    }

    if (options->contains("verb-list")) {
        activate_action("verb-list");
        return EXIT_SUCCESS;
    }

    if (options->contains("action-list")) {
        std::vector<Glib::ustring> actions = list_actions();
        for (auto action : actions) {
            std::cout << action << std::endl;
        }
        return EXIT_SUCCESS;
    }

    // For options without arguments.
    auto base = Glib::VariantBase();

    // ====================== GUI  =====================
    if (options->contains("without-gui"))    _with_gui = false;
    if (options->contains("with-gui"))       _with_gui = true;

    // Some options should preclude using gui!
    if (options->contains("query-id")      ||
        options->contains("query-x")       ||
        options->contains("query-all")     ||
        options->contains("query-y")       ||
        options->contains("query-width")   ||
        options->contains("query-height")  ||
        options->contains("export-type")   ||
        options->contains("export-file")   ||
        options->contains("export-overwrite")
        ) {
        _with_gui = false;
    }

    // ==================== ACTIONS ====================
    // Actions as an argument string: e.g.: --actions="query-id:rect1;query-x".
    // Actions will be processed in order that they are given in argument.
    Glib::ustring actions;
    if (options->contains("actions")) {
        options->lookup_value("actions", actions);

        // Split action list
        std::vector<Glib::ustring> tokens = Glib::Regex::split_simple("\\s*;\\s*", actions);
        for (auto token : tokens) {
            std::cout << token << std::endl;
            std::vector<Glib::ustring> tokens2 = Glib::Regex::split_simple("\\s*:\\s*", token);
            std::string action;
            std::string value;
            if (tokens2.size() > 0) {
                action = tokens2[0];
            }
            if (tokens2.size() > 1) {
                value = tokens2[1];
            }

            Glib::RefPtr<Gio::Action> action_ptr = lookup_action(action);
            if (action_ptr) {
                // Doesn't seem to be a way to test this using the C++ binding without Glib-CRITICAL errors.
                const  GVariantType* gtype = g_action_get_parameter_type(action_ptr->gobj());
                if (gtype) {
                    // With value.
                    Glib::VariantType type = action_ptr->get_parameter_type();
                    if (type.get_string() == "s") {
                        _command_line_actions.push_back(
                            std::make_pair( action, Glib::Variant<Glib::ustring>::create(value) ));
                    } else if (type.get_string() == "i") {
                        _command_line_actions.push_back(
                            std::make_pair( action, Glib::Variant<int>::create(std::stoi(value))));
                    } else if (type.get_string() == "d") {
                        _command_line_actions.push_back(
                            std::make_pair( action, Glib::Variant<double>::create(std::stod(value))));
                    } else {
                        std::cerr << "InkscapeApplication::on_handle_local_options: unhandled action value: "
                                  << action << ": " << type.get_string() << std::endl;
                    }
                } else {
                    // Stateless (i.e. no value).
                    _command_line_actions.push_back( std::make_pair( action, Glib::VariantBase() ) );
                }
            } else {
                std::cerr << "InkscapeApplication::on_handle_local_options: '"
                          << action << "' is not a valid action!" << std::endl;
            }
        }
    }


    // ================= OPEN/IMPORT ===================

    if (options->contains("pdf-page")) {   // Maybe useful for other file types?
        int page = 0;
        options->lookup_value("pdf-page", page);
        _command_line_actions.push_back(
            std::make_pair("open-page", Glib::Variant<int>::create(page)));
    }

    if (options->contains("convert-dpi-method")) {
        Glib::ustring method;
        options->lookup_value("convert-dpi-method", method);
        if (!method.empty()) {
            _command_line_actions.push_back(
                std::make_pair("convert-dpi-method", Glib::Variant<Glib::ustring>::create(method)));
        }
    }

    if (options->contains("no-convert-text-baseline-spacing")) _command_line_actions.push_back(std::make_pair("no-convert-baseline", base));


    // ===================== QUERY =====================

    // 'query-id' should be processed first! Idea: We could turn this into a comma separated list.
    if (options->contains("query-id")) {
        Glib::ustring query_id;
        options->lookup_value("query-id", query_id);
        if (!query_id.empty()) {
            _command_line_actions.push_back(
                std::make_pair("query-id", Glib::Variant<Glib::ustring>::create(query_id)));
        }
    }

    if (options->contains("query-all"))    _command_line_actions.push_back(std::make_pair("query-all",   base));
    if (options->contains("query-x"))      _command_line_actions.push_back(std::make_pair("query-x",     base));
    if (options->contains("query-y"))      _command_line_actions.push_back(std::make_pair("query-y",     base));
    if (options->contains("query-width"))  _command_line_actions.push_back(std::make_pair("query-width", base));
    if (options->contains("query-height")) _command_line_actions.push_back(std::make_pair("query-height",base));


    // =================== PROCESS =====================

    // Note: this won't work with --verb="FileSave,FileClose" unless some additional verb changes the file. FIXME
    // One can use --verb="FileVacuum,FileSave,FileClose".
    if (options->contains("vacuum-defs"))  _command_line_actions.push_back(std::make_pair("vacuum-defs", base));

    if (options->contains("select")) {
        Glib::ustring select;
        options->lookup_value("select", select);
        if (!select.empty()) {
            _command_line_actions.push_back(
                std::make_pair("select", Glib::Variant<Glib::ustring>::create(select)));
        }
    }

    if (options->contains("verb")) {
        Glib::ustring verb;
        options->lookup_value("verb", verb);
        if (!verb.empty()) {
            _command_line_actions.push_back(
                std::make_pair("verb", Glib::Variant<Glib::ustring>::create(verb)));
        }
    }

    // ==================== EXPORT =====================
    if (options->contains("export-file")) {
        options->lookup_value("export-file",      _file_export.export_filename);
    }

    if (options->contains("export-type")) {
        options->lookup_value("export-type",      _file_export.export_type);
    }

    if (options->contains("export-overwrite"))    _file_export.export_overwrite    = true;

    // Export - Geometry
    if (options->contains("export-area")) {
        options->lookup_value("export-area",      _file_export.export_area);
    }

    if (options->contains("export-area-drawing")) _file_export.export_area_drawing = true;
    if (options->contains("export-area-page"))    _file_export.export_area_page    = true;

    if (options->contains("export-margin")) {
        options->lookup_value("export-margin",    _file_export.export_margin);
    }

    if (options->contains("export-area-snap"))    _file_export.export_area_snap    = true;

    if (options->contains("export-width")) {
        options->lookup_value("export-width",     _file_export.export_width);
    }

    if (options->contains("export-height")) {
        options->lookup_value("export-height",    _file_export.export_height);
    }

    // Export - Options
    if (options->contains("export-id")) {
        options->lookup_value("export-id",        _file_export.export_id);
    }

    if (options->contains("export-id-only"))      _file_export.export_id_only     = true;
    if (options->contains("export-plain-svg"))    _file_export.export_plain_svg      = true;

    if (options->contains("export-dpi")) {
        options->lookup_value("export-dpi",       _file_export.export_dpi);
    }

    if (options->contains("export-ignore-filters")) _file_export.export_ignore_filters = true;
    if (options->contains("export-text-to-path"))   _file_export.export_text_to_path   = true;

    if (options->contains("export-ps-level")) {
        options->lookup_value("export-ps-level",  _file_export.export_ps_level);
    }

    if (options->contains("export-pdf-level")) {
        options->lookup_value("export-pdf-level", _file_export.export_pdf_level);
    }

    if (options->contains("export-latex"))        _file_export.export_latex       = true;
    if (options->contains("export-use-hints"))    _file_export.export_use_hints   = true;

    if (options->contains("export-background")) {
        options->lookup_value("export-background",_file_export.export_background);
    }

    if (options->contains("export-background-opacity")) {
        options->lookup_value("export-background-opacity", _file_export.export_background_opacity);
    }


    // ==================== D-BUS ======================

#ifdef WITH_DBUS
    // Before initializing extensions, we must set the DBus bus name if required
    if (options->contains("dbus-listen")) {
        std::string dbus_name;
        options->lookup_value("dbus-name", dbus_name);
        if (!dbus_name.empty()) {
            Inkscape::Extension::Dbus::dbus_set_bus_name(dbus_name.c_str());
        }
    }
#endif

    return -1; // Keep going
}

//   ========================  Actions  =========================

void
InkscapeApplication::on_new()
{
    create_window();
}

void
InkscapeApplication::on_quit()
{
    // Delete all windows (quit() doesn't do this).
    std::vector<Gtk::Window*> windows = get_windows();
    for (auto window: windows) {
        // Do something
    }

    quit();
}

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
