#include <iostream>
#include <glibmm/i18n.h>

#include "mainwindow.h"
using namespace AhoViewer;

#include "config.h"
#include "settings.h"
#include "tempdir.h"

extern const char *const ahoviewer_version;

MainWindow::MainWindow(BaseObjectType *cobj, const Glib::RefPtr<Gtk::Builder> &bldr)
  : Gtk::Window(cobj),
    m_Builder(bldr),
    m_Width(0),
    m_Height(0),
    m_HPanedMinPos(0),
    m_HPanedLastPos(0)
{
#ifndef _WIN32
    try
    {
        Glib::RefPtr<Gdk::Pixbuf> icon =
            Gdk::Pixbuf::create_from_file(DATADIR "/icons/hicolor/64x64/apps/ahoviewer.png");
        set_icon(icon);
    }
    catch (...) { }
#endif // !_WIN32

    m_Builder->get_widget_derived("ThumbnailBar",       m_ThumbnailBar);
    m_Builder->get_widget_derived("Booru::Browser",     m_BooruBrowser);
    m_Builder->get_widget_derived("ImageBox",           m_ImageBox);
    m_Builder->get_widget_derived("StatusBar",          m_StatusBar);
    m_Builder->get_widget_derived("PreferencesDialog",  m_PreferencesDialog);

    m_Builder->get_widget("AboutDialog", m_AboutDialog);
    m_Builder->get_widget("MainWindow::HPaned", m_HPaned);
    m_HPaned->property_position().signal_changed().connect(
    [ this ]()
    {
        if (!m_BooruBrowser->get_realized())
            return;

        if (m_HPaned->get_position() < m_BooruBrowser->get_min_width())
            m_HPaned->set_position(m_BooruBrowser->get_min_width());

        if (m_HPanedLastPos != m_HPaned->get_position())
        {
            m_HPanedLastPos = m_HPaned->get_position();
            m_ImageBox->queue_draw_image();
        }
    });

    m_UIManager = Glib::RefPtr<Gtk::UIManager>::cast_static(m_Builder->get_object("UIManager"));

    m_LocalImageList = std::make_shared<ImageList>(m_ThumbnailBar);
    m_LocalImageList->signal_archive_error().connect([ this ](const std::string e) { m_StatusBar->set_message(e); });
    m_LocalImageList->signal_load_success().connect([ this ]() { set_active_imagelist(m_LocalImageList); });
    m_LocalImageList->signal_extractor_progress().connect([ this ](size_t c, size_t t)
    {
        m_StatusBar->set_message("Extracting");
        m_StatusBar->set_progress(static_cast<double>(c) / t);
        while (Gtk::Main::events_pending())
            Gtk::Main::iteration();
    });

    m_BooruBrowser->signal_page_changed().connect([ this ](Booru::Page *page)
            { set_active_imagelist(page ? page->get_imagelist() : m_LocalImageList); });
    m_BooruBrowser->signal_realize().connect([ this ]()
            { m_HPaned->set_position(Settings.get_int("BooruWidth")); });
    m_BooruBrowser->signal_entry_blur().connect([ this ]()
            { m_ImageBox->grab_focus(); });

    m_ImageBox->signal_slideshow_ended().connect(
            sigc::mem_fun(*this, &MainWindow::on_toggle_slideshow));

    m_PreferencesDialog->signal_bg_color_set().connect(
            sigc::mem_fun(m_ImageBox, &ImageBox::update_background_color));
    m_PreferencesDialog->signal_cursor_hide_delay_changed().connect(
            sigc::mem_fun(m_ImageBox, &ImageBox::cursor_timeout));
    m_PreferencesDialog->signal_cache_size_changed().connect(
            sigc::mem_fun(*this, &MainWindow::on_cache_size_changed));
    m_PreferencesDialog->signal_slideshow_delay_changed().connect(
            sigc::mem_fun(m_ImageBox, &ImageBox::reset_slideshow));
    m_PreferencesDialog->get_site_editor()->signal_edited().connect(
            sigc::mem_fun(m_BooruBrowser, &Booru::Browser::update_combobox_model));
    m_PreferencesDialog->get_keybinding_editor()->signal_edited().connect(
            sigc::mem_fun(*this, &MainWindow::on_accel_edited));

    // About Dialog {{{
    m_AboutDialog->signal_response().connect([ this ](const int) { m_AboutDialog->hide(); });

    try
    {
        Glib::RefPtr<Gdk::Pixbuf> logo =
#ifdef _WIN32
            Gdk::Pixbuf::create_from_file("share/pixmaps/ahoviewer/ahoviewer-about-logo.png");
#else
            Gdk::Pixbuf::create_from_file(DATADIR "/pixmaps/ahoviewer/ahoviewer-about-logo.png");
#endif // _WIN32
        m_AboutDialog->set_logo(logo);
    }
    catch (...) { }

    m_AboutDialog->set_name(PACKAGE);
    m_AboutDialog->set_version(ahoviewer_version);
    m_AboutDialog->set_copyright("Copyright \302\251 2013-2015 ahoka");
    m_AboutDialog->set_website(PACKAGE_URL);
    // }}}

    // Setup drag and drop
    std::vector<Gtk::TargetEntry> dropTargets = { Gtk::TargetEntry("text/uri-list") };
    drag_dest_set(dropTargets, Gtk::DEST_DEFAULT_ALL, Gdk::ACTION_COPY | Gdk::ACTION_MOVE);

    // Call this to make sure it is rendered in the main thread.
    Image::get_missing_pixbuf();

    create_actions();

    // Restore window geometry
    int x, y, w, h;
    if (Settings.get_geometry(x, y, w, h))
    {
        set_position(Gtk::WIN_POS_NONE);
        set_default_size(w, h);

        show_all();
        move(x, y);
    }
    else
    {
        show_all();
    }

    if (Settings.get_bool("StartFullscreen"))
        m_ActionGroup->get_action("ToggleFullscreen")->activate();
}

MainWindow::~MainWindow()
{
    delete m_PreferencesDialog;
}

void MainWindow::open_file(const std::string &path, const int index, const bool restore)
{
    if (path.empty())
        return;

    std::string absolutePath, error;

    if (Glib::path_is_absolute(path))
    {
        absolutePath = std::string(path);
    }
    else
    {
        try { absolutePath = Glib::filename_from_uri(path); }
        catch (Glib::ConvertError) { }

        // From relative path
        if (absolutePath.empty())
            absolutePath = Glib::build_filename(Glib::get_current_dir(), path);
    }

    if (!m_LocalImageList->load(absolutePath, error, index))
    {
        std::string uri = Glib::filename_to_uri(absolutePath);

        if (Gtk::RecentManager::get_default()->has_item(uri))
            Gtk::RecentManager::get_default()->remove_item(uri);

        m_StatusBar->clear_progress();
        m_StatusBar->set_message(error);
    }
    else
    {
        if (Settings.get_bool("StoreRecentFiles"))
            Gtk::RecentManager::get_default()->add_item(Glib::filename_to_uri(absolutePath));

        Glib::RefPtr<Gtk::ToggleAction> tbAction =
            Glib::RefPtr<Gtk::ToggleAction>::cast_static(m_ActionGroup->get_action("ToggleThumbnailBar"));

        // Show the thumbnail bar when opening a new file
        if (!restore && !tbAction->get_active())
            tbAction->set_active(true);
        else
            update_widgets_visibility();

        present();
    }
}

void MainWindow::restore_last_file()
{
    if (Settings.get_bool("RememberLastFile"))
    {
        std::string path = Settings.get_string("LastOpenFile");

        if (!path.empty())
            open_file(path, Settings.get_int("ArchiveIndex"), true);
    }
}

void MainWindow::get_drawable_area_size(int &w, int &h) const
{
    get_size(w, h);

    if (m_ThumbnailBar->get_visible())
        w -= m_ThumbnailBar->size_request().width;

    if (m_BooruBrowser->get_visible())
        w -= m_HPaned->get_position() + m_HPaned->get_handle_window()->get_width();

    if (m_MenuBar->get_visible())
        h -= m_MenuBar->size_request().height;

    if (m_StatusBar->get_visible())
        h -= m_StatusBar->size_request().height;
}

void MainWindow::on_realize()
{
    Gtk::Window::on_realize();

    update_widgets_visibility();
}

void MainWindow::on_check_resize()
{
    int w = get_allocation().get_width(),
        h = get_allocation().get_height();

    // Make sure we really need to redraw
    if (w != m_Width || h != m_Height)
        m_ImageBox->queue_draw_image();

    m_Width = w;
    m_Height = h;

    Gtk::Window::on_check_resize();
}

bool MainWindow::on_delete_event(GdkEventAny*)
{
    on_quit();
    return true;
}

void MainWindow::on_drag_data_received(const Glib::RefPtr<Gdk::DragContext> &ctx, int, int,
                                       const Gtk::SelectionData &data, guint, guint time)
{
    std::vector<std::string> uris = data.get_uris();

    if (uris.size())
    {
        std::string uri;
        // In case the URI is not using the file scheme
        try
        {
            uri = Glib::filename_from_uri(uris[0]);
        }
        catch (const Glib::ConvertError &ex)
        {
            std::cerr << ex.what() << std::endl;
        }

        if (!uri.empty())
        {
            open_file(uri);
            ctx->drag_finish(true, false, time);
        }
    }
    else
    {
        ctx->drag_finish(false, false, time);
    }
}

bool MainWindow::on_key_press_event(GdkEventKey *e)
{
    // Hacky method to allow arrow key accelerators for the imagebox
    if (m_ImageBox->has_focus() && (e->keyval == GDK_KEY_Up   ||
                                    e->keyval == GDK_KEY_Down ||
                                    e->keyval == GDK_KEY_Left ||
                                    e->keyval == GDK_KEY_Right))
    {
        GtkAccelGroupEntry *entries;
        if ((entries = gtk_accel_group_query(m_UIManager->get_accel_group()->gobj(),
                    e->keyval, static_cast<GdkModifierType>(e->state), NULL)))
        {
            std::string path(g_quark_to_string(entries[0].accel_path_quark));
            m_ActionGroup->get_action(path.substr(path.rfind('/') + 1))->activate();

            return true;
        }
    }
    else if (m_BooruBrowser->get_tag_entry()->has_focus() && !(e->state & GDK_CONTROL_MASK))
    {
        return m_BooruBrowser->get_tag_entry()->event(reinterpret_cast<GdkEvent*>(e));
    }

    return Gtk::Window::on_key_press_event(e);
}

void MainWindow::set_active_imagelist(std::shared_ptr<ImageList> imageList)
{
    m_ImageListConn.disconnect();
    m_ImageListClearedConn.disconnect();
    m_ActiveImageList = imageList;

    m_ImageListConn = m_ActiveImageList->signal_changed().connect(
            sigc::mem_fun(*this, &MainWindow::on_imagelist_changed));
    m_ImageListClearedConn = m_ActiveImageList->signal_cleared().connect(
            sigc::mem_fun(*this, &MainWindow::clear));

    if (!m_ActiveImageList->empty())
        m_ImageBox->set_image(m_ActiveImageList->get_current());
    else
        clear();

    update_title();
    set_sensitives();
}

void MainWindow::save_window_geometry()
{
    if (is_fullscreen())
        return;

    int x, y, w, h;

    get_size(w, h);
    get_position(x, y);

    Settings.set_geometry(x, y, w, h);
}

/**
 * Creates the UIManager and sets up all of the actions.
 * Also adds the menubar to the table.
 **/
void MainWindow::create_actions()
{
    // Create the action group for the UIManager
    m_ActionGroup = Gtk::ActionGroup::create(PACKAGE);

    // Menu actions {{{
    m_ActionGroup->add(Gtk::Action::create("FileMenu", _("_File")));
    m_RecentAction = Gtk::RecentAction::create("RecentMenu", Gtk::Stock::DND_MULTIPLE,
                _("_Open Recent"), "", Gtk::RecentManager::get_default());
    m_RecentAction->signal_item_activated().connect(sigc::mem_fun(*this, &MainWindow::on_open_recent_file));
    m_ActionGroup->add(m_RecentAction);
    m_ActionGroup->add(Gtk::Action::create("ViewMenu", _("_View")));
    m_ActionGroup->add(Gtk::Action::create("ZoomMenu", _("_Zoom")));
    m_ActionGroup->add(Gtk::Action::create("GoMenu", _("_Go")));
    m_ActionGroup->add(Gtk::Action::create("HelpMenu", _("_Help")));
    // }}}

    // Normal actions {{{
    m_ActionGroup->add(Gtk::Action::create("OpenFile", Gtk::Stock::OPEN,
                _("_Open File"), _("Open a file")),
            Gtk::AccelKey(Settings.get_keybinding("File", "OpenFile")),
            sigc::mem_fun(*this, &MainWindow::on_open_file_dialog));
    m_ActionGroup->add(Gtk::Action::create("Preferences", Gtk::Stock::PREFERENCES,
                _("_Preferences"), _("Open the preferences dialog")),
            Gtk::AccelKey(Settings.get_keybinding("File", "Preferences")),
            sigc::mem_fun(m_PreferencesDialog, &PreferencesDialog::show_all));
    m_ActionGroup->add(Gtk::Action::create("Close", Gtk::Stock::CLOSE,
                _("_Close"), _("Close local image list or booru tab")),
            Gtk::AccelKey(Settings.get_keybinding("File", "Close")),
            sigc::mem_fun(*this, &MainWindow::on_close));
    m_ActionGroup->add(Gtk::Action::create("Quit", Gtk::Stock::QUIT,
                _("_Quit"), _("Quit " PACKAGE)),
            Gtk::AccelKey(Settings.get_keybinding("File", "Quit")),
            sigc::mem_fun(*this, &MainWindow::on_quit));

    m_ActionGroup->add(Gtk::Action::create("ZoomIn", Gtk::Stock::ZOOM_IN, _("Zoom _In")),
            Gtk::AccelKey(Settings.get_keybinding("Zoom", "ZoomIn")),
            sigc::mem_fun(m_ImageBox, &ImageBox::on_zoom_in));
    m_ActionGroup->add(Gtk::Action::create("ZoomOut", Gtk::Stock::ZOOM_OUT, _("Zoom _Out")),
            Gtk::AccelKey(Settings.get_keybinding("Zoom", "ZoomOut")),
            sigc::mem_fun(m_ImageBox, &ImageBox::on_zoom_out));
    m_ActionGroup->add(Gtk::Action::create("ResetZoom", Gtk::Stock::ZOOM_100,
                _("_Reset Zoom"), _("Reset image to it's original size")),
            Gtk::AccelKey(Settings.get_keybinding("Zoom", "ResetZoom")),
            sigc::mem_fun(m_ImageBox, &ImageBox::on_reset_zoom));

    m_ActionGroup->add(Gtk::Action::create("NextImage", Gtk::Stock::GO_FORWARD,
                _("_Next Image"), _("Go to next image")),
            Gtk::AccelKey(Settings.get_keybinding("Navigation", "NextImage")),
            sigc::mem_fun(*this, &MainWindow::on_next_image));
    m_ActionGroup->add(Gtk::Action::create("PreviousImage", Gtk::Stock::GO_BACK,
                _("_Previous Image"), _("Go to previous image")),
            Gtk::AccelKey(Settings.get_keybinding("Navigation", "PreviousImage")),
            sigc::mem_fun(*this, &MainWindow::on_previous_image));
    m_ActionGroup->add(Gtk::Action::create("FirstImage", Gtk::Stock::GOTO_FIRST,
                _("_First Image"), _("Go to first image")),
            Gtk::AccelKey(Settings.get_keybinding("Navigation", "FirstImage")),
            sigc::mem_fun(*this, &MainWindow::on_first_image));
    m_ActionGroup->add(Gtk::Action::create("LastImage", Gtk::Stock::GOTO_LAST,
                _("_Last Image"), _("Go to last image")),
            Gtk::AccelKey(Settings.get_keybinding("Navigation", "LastImage")),
            sigc::mem_fun(*this, &MainWindow::on_last_image));

    m_ActionGroup->add(Gtk::Action::create("About", Gtk::Stock::ABOUT,
                _("_About"), _("View information about " PACKAGE)),
            sigc::mem_fun(m_AboutDialog, &Gtk::AboutDialog::show));

    m_ActionGroup->add(Gtk::Action::create("ScrollUp"),
            Gtk::AccelKey(Settings.get_keybinding("Scroll", "ScrollUp")),
            sigc::mem_fun(m_ImageBox, &ImageBox::on_scroll_up));
    m_ActionGroup->add(Gtk::Action::create("ScrollDown"),
            Gtk::AccelKey(Settings.get_keybinding("Scroll", "ScrollDown")),
            sigc::mem_fun(m_ImageBox, &ImageBox::on_scroll_down));
    m_ActionGroup->add(Gtk::Action::create("ScrollLeft"),
            Gtk::AccelKey(Settings.get_keybinding("Scroll", "ScrollLeft")),
            sigc::mem_fun(m_ImageBox, &ImageBox::on_scroll_left));
    m_ActionGroup->add(Gtk::Action::create("ScrollRight"),
            Gtk::AccelKey(Settings.get_keybinding("Scroll", "ScrollRight")),
            sigc::mem_fun(m_ImageBox, &ImageBox::on_scroll_right));

    m_ActionGroup->add(Gtk::Action::create("NewTab", Gtk::Stock::ADD, _("New Tab"), _("New Tab")),
            Gtk::AccelKey(Settings.get_keybinding("BooruBrowser", "NewTab")),
            sigc::mem_fun(m_BooruBrowser, &Booru::Browser::on_new_tab));
    m_ActionGroup->add(Gtk::Action::create("SaveImage", Gtk::Stock::SAVE_AS,
            _("Save Image as..."), _("Save the selected image")),
            Gtk::AccelKey(Settings.get_keybinding("BooruBrowser", "SaveImage")),
            sigc::mem_fun(m_BooruBrowser, &Booru::Browser::on_save_image));
    m_ActionGroup->add(Gtk::Action::create("SaveImages", Gtk::Stock::SAVE, _("Save Images"), _("Save Images")),
            Gtk::AccelKey(Settings.get_keybinding("BooruBrowser", "SaveImages")),
            sigc::mem_fun(m_BooruBrowser, &Booru::Browser::on_save_images));

    m_ActionGroup->add(Gtk::Action::create("ViewPost", Gtk::Stock::PROPERTIES,
            _("View Post"), _("View the selected image's post in your default web browser")),
            Gtk::AccelKey(Settings.get_keybinding("BooruBrowser", "ViewPost")),
            sigc::mem_fun(m_BooruBrowser, &Booru::Browser::on_view_post));
    m_ActionGroup->add(Gtk::Action::create("CopyImageURL", Gtk::Stock::COPY,
            _("Copy Image URL"), _("Copy the selected image's URL to your clipboard")),
            Gtk::AccelKey(Settings.get_keybinding("BooruBrowser", "CopyImageURL")),
            sigc::mem_fun(m_BooruBrowser, &Booru::Browser::on_copy_image_url));
    // }}}

    // Toggle actions {{{
    Glib::RefPtr<Gtk::ToggleAction> toggleAction;
    toggleAction = Gtk::ToggleAction::create("ToggleFullscreen",
            _("_Fullscreen"), _("Toggle fullscreen mode"));
    m_ActionGroup->add(toggleAction,
            Gtk::AccelKey(Settings.get_keybinding("UserInterface", "ToggleFullscreen")),
            sigc::mem_fun(*this, &MainWindow::on_toggle_fullscreen));

    toggleAction = Gtk::ToggleAction::create("ToggleMangaMode",
            _("_Manga Mode"), _("Toggle manga mode"), Settings.get_bool("MangaMode"));
    m_ActionGroup->add(toggleAction,
            Gtk::AccelKey(Settings.get_keybinding("ViewMode", "ToggleMangaMode")),
            sigc::mem_fun(*this, &MainWindow::on_toggle_manga_mode));

    toggleAction = Gtk::ToggleAction::create("ToggleMenuBar",
            _("_Menubar"), _("Toggle menubar visibility"), Settings.get_bool("MenuBarVisible"));
    m_ActionGroup->add(toggleAction,
            Gtk::AccelKey(Settings.get_keybinding("UserInterface", "ToggleMenuBar")),
            sigc::mem_fun(*this, &MainWindow::on_toggle_menu_bar));

    toggleAction = Gtk::ToggleAction::create("ToggleStatusBar",
            _("_Statusbar"), _("Toggle statusbar visibility"), Settings.get_bool("StatusBarVisible"));
    m_ActionGroup->add(toggleAction,
            Gtk::AccelKey(Settings.get_keybinding("UserInterface", "ToggleStatusBar")),
            sigc::mem_fun(*this, &MainWindow::on_toggle_status_bar));

    toggleAction = Gtk::ToggleAction::create("ToggleScrollbars",
            _("_Scrollbars"), _("Toggle scrollbar visibility"), Settings.get_bool("ScrollbarsVisible"));
    m_ActionGroup->add(toggleAction,
            Gtk::AccelKey(Settings.get_keybinding("UserInterface", "ToggleScrollbars")),
            sigc::mem_fun(m_ImageBox, &ImageBox::on_toggle_scrollbars));

    toggleAction = Gtk::ToggleAction::create("ToggleBooruBrowser",
            _("_Booru Browser"), _("Toggle booru browser visibility"), Settings.get_bool("BooruBrowserVisible"));
    toggleAction->set_draw_as_radio(true);
    m_ActionGroup->add(toggleAction,
            Gtk::AccelKey(Settings.get_keybinding("UserInterface", "ToggleBooruBrowser")),
            sigc::mem_fun(*this, &MainWindow::on_toggle_booru_browser));

    toggleAction = Gtk::ToggleAction::create("ToggleThumbnailBar",
            _("_Thumbnail Bar"), _("Toggle thumbnailbar visibility"), Settings.get_bool("ThumbnailBarVisible"));
    toggleAction->set_draw_as_radio(true);
    m_ActionGroup->add(toggleAction,
            Gtk::AccelKey(Settings.get_keybinding("UserInterface", "ToggleThumbnailBar")),
            sigc::mem_fun(*this, &MainWindow::on_toggle_thumbnail_bar));

    toggleAction = Gtk::ToggleAction::create("ToggleHideAll",
            _("_Hide All"), _("Toggle visibility of all interface widgets"), Settings.get_bool("HideAll"));
    m_ActionGroup->add(toggleAction,
            Gtk::AccelKey(Settings.get_keybinding("UserInterface", "ToggleHideAll")),
            sigc::mem_fun(*this, &MainWindow::on_toggle_hide_all));

    toggleAction = Gtk::ToggleAction::create("ToggleSlideshow",
            _("_Slideshow"), _("Start or stop the slideshow"));
    m_ActionGroup->add(toggleAction,
            Gtk::AccelKey(Settings.get_keybinding("Navigation", "ToggleSlideshow")),
            sigc::mem_fun(*this, &MainWindow::on_toggle_slideshow));
    // }}}

    // Radio actions {{{
    Gtk::RadioAction::Group zoomModeGroup;
    Glib::RefPtr<Gtk::RadioAction> radioAction;

    radioAction = Gtk::RadioAction::create(zoomModeGroup, "AutoFitMode",
            _("_Auto Fit Mode"), _("Fit the image to either the height or width of the window"));
    radioAction->property_value().set_value(static_cast<int>(ImageBox::ZoomMode::AUTO_FIT));
    m_ActionGroup->add(radioAction,
            Gtk::AccelKey(Settings.get_keybinding("ViewMode", "AutoFitMode")),
            sigc::bind(sigc::mem_fun(m_ImageBox, &ImageBox::set_zoom_mode), ImageBox::ZoomMode::AUTO_FIT));
    radioAction = Gtk::RadioAction::create(zoomModeGroup, "FitWidthMode",
            _("Fit _Width Mode"), _("Fit the image to the width of the window"));
    radioAction->property_value().set_value(static_cast<int>(ImageBox::ZoomMode::FIT_WIDTH));
    m_ActionGroup->add(radioAction,
            Gtk::AccelKey(Settings.get_keybinding("ViewMode", "FitWidthMode")),
            sigc::bind(sigc::mem_fun(m_ImageBox, &ImageBox::set_zoom_mode), ImageBox::ZoomMode::FIT_WIDTH));
    radioAction = Gtk::RadioAction::create(zoomModeGroup, "FitHeightMode",
            _("Fit _Height Mode"), _("Fit the image to the height of the window"));
    radioAction->property_value().set_value(static_cast<int>(ImageBox::ZoomMode::FIT_HEIGHT));
    m_ActionGroup->add(radioAction,
            Gtk::AccelKey(Settings.get_keybinding("ViewMode", "FitHeightMode")),
            sigc::bind(sigc::mem_fun(m_ImageBox, &ImageBox::set_zoom_mode), ImageBox::ZoomMode::FIT_HEIGHT));
    radioAction = Gtk::RadioAction::create(zoomModeGroup, "ManualZoomMode",
            _("_Manual Zoom"), _("Display the image at it's original size, with the ability to zoom in and out"));
    radioAction->property_value().set_value(static_cast<int>(ImageBox::ZoomMode::MANUAL));
    m_ActionGroup->add(radioAction,
            Gtk::AccelKey(Settings.get_keybinding("ViewMode", "ManualZoomMode")),
            sigc::bind(sigc::mem_fun(m_ImageBox, &ImageBox::set_zoom_mode), ImageBox::ZoomMode::MANUAL));

    radioAction->set_current_value(static_cast<int>(m_ImageBox->get_zoom_mode()));
    // }}}

    m_UIManager->insert_action_group(m_ActionGroup);

    // Setup tooltip handling
    m_UIManager->signal_connect_proxy().connect(sigc::mem_fun(*this, &MainWindow::on_connect_proxy));

    // Create the recent menu filter
    m_RecentAction->set_show_tips();
    m_RecentAction->set_sort_type(Gtk::RECENT_SORT_MRU);

    Gtk::RecentFilter filter;
    filter.add_pixbuf_formats();

    for (const std::string &mimeType : Archive::MimeTypes)
        filter.add_mime_type(mimeType);

    for (const std::string &ext : Archive::FileExtensions)
        filter.add_pattern("*." + ext);

#ifdef HAVE_GSTREAMER
    filter.add_mime_type("video/webm");
#endif // HAVE_GSTREAMER

    m_RecentAction->add_filter(filter);

    // Add the accel group to the window.
    add_accel_group(m_UIManager->get_accel_group());

    // Finally attach the menubar to the main window's table
    Gtk::Table *table = nullptr;
    m_Builder->get_widget("MainWindow::Table", table);

    m_MenuBar = static_cast<Gtk::MenuBar*>(m_UIManager->get_widget("/MenuBar"));
    table->attach(*m_MenuBar, 0, 2, 0, 1, Gtk::EXPAND | Gtk::FILL, Gtk::SHRINK | Gtk::FILL);
}

void MainWindow::update_widgets_visibility()
{
    bool hideAll = Settings.get_bool("HideAll");

    m_MenuBar->set_visible(!hideAll && Settings.get_bool("MenuBarVisible"));
    m_StatusBar->set_visible(!hideAll && Settings.get_bool("StatusBarVisible"));
    m_BooruBrowser->set_visible(!hideAll && Settings.get_bool("BooruBrowserVisible"));
    m_ThumbnailBar->set_visible(!hideAll && Settings.get_bool("ThumbnailBarVisible") &&
                                !m_BooruBrowser->get_visible() && !m_LocalImageList->empty());

    m_ImageBox->queue_draw_image();
    set_sensitives();
}

void MainWindow::set_sensitives()
{
    std::vector<std::string> names =
    {
        "ToggleMenuBar",
        "ToggleStatusBar",
        "ToggleScrollbars",
        "ToggleBooruBrowser",
    };

    for (const std::string &s : names)
        Glib::RefPtr<Gtk::ToggleAction>::cast_static(m_ActionGroup->get_action(s))->
            set_sensitive(!Settings.get_bool("HideAll"));

    names =
    {
        "NextImage",
        "PreviousImage",
        "FirstImage",
        "LastImage",
        "ToggleSlideshow",
    };

    for (const std::string &s : names)
    {
        bool sens = !!m_ActiveImageList && !m_ActiveImageList->empty();

        if (s == "NextImage")
            sens = sens && m_ActiveImageList->can_go_next();
        else if (s == "PreviousImage")
            sens = sens && m_ActiveImageList->can_go_previous();

        Glib::RefPtr<Gtk::ToggleAction>::cast_static(m_ActionGroup->get_action(s))->set_sensitive(sens);
    }

    Booru::Page *page = m_BooruBrowser->get_active_page();
    bool local = !m_LocalImageList->empty() && m_LocalImageList == m_ActiveImageList,
         booru = page && (m_BooruBrowser->get_visible() || page->get_imagelist() == m_ActiveImageList);

    m_ActionGroup->get_action("ToggleThumbnailBar")->set_sensitive(
            !Settings.get_bool("HideAll") && !m_LocalImageList->empty());

    m_ActionGroup->get_action("Close")->set_sensitive(local || booru);
    m_ActionGroup->get_action("NewTab")->set_sensitive(m_BooruBrowser->get_visible());
    m_ActionGroup->get_action("SaveImage")->set_sensitive(booru && !page->get_imagelist()->empty());
    m_ActionGroup->get_action("SaveImages")->set_sensitive(booru && !page->get_imagelist()->empty());
    m_ActionGroup->get_action("ViewPost")->set_sensitive(booru && !page->get_imagelist()->empty());
    m_ActionGroup->get_action("CopyImageURL")->set_sensitive(booru && !page->get_imagelist()->empty());
}

void MainWindow::update_title()
{
    if (m_ActiveImageList && !m_ActiveImageList->empty())
    {
        std::ostringstream ss;

        if (m_ImageBox->is_slideshow_running())
            ss << "[SLIDESHOW] ";

        ss << "[" << m_ActiveImageList->get_index() + 1 << " / "
           << m_ActiveImageList->get_size() << "] "
           << m_ActiveImageList->get_current()->get_filename()
           << " - " PACKAGE;
        set_title(ss.str());


        m_StatusBar->set_page_info(m_ActiveImageList->get_index() + 1, m_ActiveImageList->get_size());
        m_StatusBar->set_filename(m_ActiveImageList->get_current()->get_filename());
    }
    else
    {
        set_title(PACKAGE);
    }
}

void MainWindow::clear()
{
    m_ImageBox->clear_image();
    m_StatusBar->clear_page_info();
    m_StatusBar->clear_filename();
}

bool MainWindow::is_fullscreen() const
{
    return get_window() && (get_window()->get_state() & Gdk::WINDOW_STATE_FULLSCREEN) != 0;
}

void MainWindow::on_imagelist_changed(const std::shared_ptr<Image> &image)
{
    m_ImageBox->set_image(image);
    update_title();
    set_sensitives();
}

void MainWindow::on_cache_size_changed()
{
    m_LocalImageList->on_cache_size_changed();
    for (const Booru::Page *p : m_BooruBrowser->get_pages())
        p->get_imagelist()->on_cache_size_changed();
}

void MainWindow::on_accel_edited(const std::string &accelPath, const std::string &actionName)
{
    Glib::RefPtr<Gtk::Action> action = m_ActionGroup->get_action(actionName);
    action->set_accel_path(accelPath);
    action->connect_accelerator();
}

void MainWindow::on_connect_proxy(const Glib::RefPtr<Gtk::Action> &action, Gtk::Widget *w)
{
    if (dynamic_cast<Gtk::MenuItem*>(w) && !action->get_tooltip().empty())
    {
        static_cast<Gtk::MenuItem*>(w)->signal_select().connect(
                sigc::bind(sigc::mem_fun(m_StatusBar, &StatusBar::set_message),
                    action->get_tooltip(), StatusBar::Priority::TOOLTIP, 0));
        static_cast<Gtk::MenuItem*>(w)->signal_deselect().connect(
                sigc::mem_fun(m_StatusBar, &StatusBar::clear_message));
    }
}

void MainWindow::on_open_file_dialog()
{
    Gtk::FileChooserDialog dialog(*this, "Open", Gtk::FILE_CHOOSER_ACTION_OPEN);
    Gtk::FileFilter filter, imageFilter, archiveFilter;

    dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
    dialog.add_button("Open", Gtk::RESPONSE_OK);

    filter.set_name("All Files");
    imageFilter.set_name("All Images");
    archiveFilter.set_name("All Archives");

    filter.add_pixbuf_formats();
    imageFilter.add_pixbuf_formats();

#ifdef HAVE_GSTREAMER
    filter.add_mime_type("video/webm");
    imageFilter.add_mime_type("video/webm");
#endif // HAVE_GSTREAMER

    for (const std::string &mimeType : Archive::MimeTypes)
    {
        filter.add_mime_type(mimeType);
        archiveFilter.add_mime_type(mimeType);
    }

    for (const std::string &ext : Archive::FileExtensions)
    {
        filter.add_pattern("*." + ext);
        archiveFilter.add_pattern("*." + ext);
    }

    dialog.add_filter(filter);
    dialog.add_filter(imageFilter);
    dialog.add_filter(archiveFilter);

    if (!m_LocalImageList->empty())
    {
        std::string path = m_LocalImageList->from_archive() ?
            m_LocalImageList->get_archive().get_path() : m_LocalImageList->get_current()->get_path();
        dialog.set_filename(path);
    }

    if (dialog.run() == Gtk::RESPONSE_OK)
        open_file(dialog.get_filename());
}

void MainWindow::on_open_recent_file()
{
    Glib::ustring uri = m_RecentAction->get_current_uri();

    if (!uri.empty())
        open_file(Glib::filename_from_uri(uri));
}

void MainWindow::on_close()
{
    if (m_ActiveImageList == m_LocalImageList)
    {
        m_LocalImageList->clear();

        Booru::Page *page = m_BooruBrowser->get_active_page();

        if (page && !page->get_imagelist()->empty())
        {
            Glib::RefPtr<Gtk::ToggleAction>::cast_static(m_ActionGroup->
                    get_action("ToggleBooruBrowser"))->set_active();
            set_active_imagelist(page->get_imagelist());
        }
        else
        {
            update_title();
            update_widgets_visibility();
        }
    }
    else
    {
        m_BooruBrowser->on_close_tab();
    }
}

void MainWindow::on_quit()
{
    if (m_BooruBrowser->get_realized())
    {
        Settings.set("TagViewPosition", m_BooruBrowser->get_position());
        Settings.set("BooruWidth", m_HPaned->get_position());
    }

    Settings.set("SelectedBooru", m_BooruBrowser->get_selected_booru());

    for (std::shared_ptr<Booru::Site> site : Settings.get_sites())
        site->save_tags();

    save_window_geometry();

    if (Settings.get_bool("RememberLastFile") && !m_LocalImageList->empty())
    {
        std::string path = m_LocalImageList->get_current()->get_path();

        if (m_LocalImageList->from_archive())
        {
            path = m_LocalImageList->get_archive().get_path();
            Settings.set("ArchiveIndex", static_cast<int>(m_LocalImageList->get_index()));
        }
        else
        {
            Settings.remove("ArchiveIndex");
        }

        Settings.set("LastOpenFile", path);
    }
    else
    {
        Settings.remove("LastOpenFile");
        Settings.remove("ArchiveIndex");
    }

    if (Settings.get_bool("RememberLastSavePath"))
        Settings.set("LastSavePath", m_BooruBrowser->get_last_save_path());
    else
        Settings.remove("LastSavePath");

    hide();
}

void MainWindow::on_toggle_fullscreen()
{
    if (is_fullscreen())
    {
        if (Settings.get_bool("HideAllFullscreen"))
            Glib::RefPtr<Gtk::ToggleAction>::cast_static(m_ActionGroup->
                    get_action("ToggleHideAll"))->set_active(false);

        unfullscreen();
    }
    else
    {
        if (Settings.get_bool("HideAllFullscreen"))
            Glib::RefPtr<Gtk::ToggleAction>::cast_static(m_ActionGroup->
                    get_action("ToggleHideAll"))->set_active();

        // Save this here in case the program is closed when fullscreen
        save_window_geometry();
        fullscreen();
    }
}

void MainWindow::on_toggle_manga_mode()
{
    bool active = Glib::RefPtr<Gtk::ToggleAction>::cast_static(m_ActionGroup->
            get_action("ToggleMangaMode"))->get_active();

    Settings.set("MangaMode", active);

    m_ImageBox->queue_draw_image(true);
}

void MainWindow::on_toggle_menu_bar()
{
    Glib::RefPtr<Gtk::ToggleAction> a =
        Glib::RefPtr<Gtk::ToggleAction>::cast_static(m_ActionGroup->get_action("ToggleMenuBar"));

    Settings.set("MenuBarVisible", a->get_active());

    update_widgets_visibility();
}

void MainWindow::on_toggle_status_bar()
{
    Glib::RefPtr<Gtk::ToggleAction> a =
        Glib::RefPtr<Gtk::ToggleAction>::cast_static(m_ActionGroup->get_action("ToggleStatusBar"));

    Settings.set("StatusBarVisible", a->get_active());

    update_widgets_visibility();
}

void MainWindow::on_toggle_booru_browser()
{
    Glib::RefPtr<Gtk::ToggleAction> tbAction =
        Glib::RefPtr<Gtk::ToggleAction>::cast_static(m_ActionGroup->get_action("ToggleThumbnailBar"));
    Glib::RefPtr<Gtk::ToggleAction> bbAction =
        Glib::RefPtr<Gtk::ToggleAction>::cast_static(m_ActionGroup->get_action("ToggleBooruBrowser"));

    Settings.set("BooruBrowserVisible", bbAction->get_active());

    if (bbAction->get_active())
    {
        if (!Settings.get_bool("HideAll"))
            m_BooruBrowser->get_tag_entry()->grab_focus();

        if (m_BooruBrowser->get_active_page() &&
            m_BooruBrowser->get_active_page()->get_imagelist() != m_ActiveImageList)
            set_active_imagelist(m_BooruBrowser->get_active_page()->get_imagelist());

        if (tbAction->get_active())
        {
            tbAction->set_active(false);
            return;
        }

    }

    update_widgets_visibility();
}

void MainWindow::on_toggle_thumbnail_bar()
{
    Glib::RefPtr<Gtk::ToggleAction> tbAction =
        Glib::RefPtr<Gtk::ToggleAction>::cast_static(m_ActionGroup->get_action("ToggleThumbnailBar"));
    Glib::RefPtr<Gtk::ToggleAction> bbAction =
        Glib::RefPtr<Gtk::ToggleAction>::cast_static(m_ActionGroup->get_action("ToggleBooruBrowser"));

    Settings.set("ThumbnailBarVisible", tbAction->get_active());

    if (tbAction->get_active())
    {
        if (m_ActiveImageList != m_LocalImageList)
            set_active_imagelist(m_LocalImageList);

        if (bbAction->get_active())
        {
            bbAction->set_active(false);
            return;
        }
    }

    update_widgets_visibility();
}

void MainWindow::on_toggle_hide_all()
{
    Glib::RefPtr<Gtk::ToggleAction> a =
        Glib::RefPtr<Gtk::ToggleAction>::cast_static(m_ActionGroup->get_action("ToggleHideAll"));

    Settings.set("HideAll", a->get_active());

    update_widgets_visibility();
}

void MainWindow::on_next_image()
{
    m_ActiveImageList->go_next();
}

void MainWindow::on_previous_image()
{
    m_ActiveImageList->go_previous();
}

void MainWindow::on_first_image()
{
    m_ActiveImageList->go_first();
}

void MainWindow::on_last_image()
{
    m_ActiveImageList->go_last();
}

void MainWindow::on_toggle_slideshow()
{
    m_ImageBox->toggle_slideshow();
    update_title();
}
