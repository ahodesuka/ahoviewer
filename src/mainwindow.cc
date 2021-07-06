#include "mainwindow.h"
using namespace AhoViewer;

#include "application.h"
#include "booru/browser.h"
#include "booru/infobox.h"
#include "config.h"
#include "image.h"
#include "imagebox.h"
#include "imagelist.h"
#include "preferences.h"
#include "settings.h"
#include "statusbar.h"
#include "tempdir.h"
#include "thumbnailbar.h"

#include <glibmm/i18n.h>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <utility>

extern const char* const ahoviewer_version;

PreferencesDialog* MainWindow::m_PreferencesDialog{ nullptr };
Gtk::AboutDialog* MainWindow::m_AboutDialog{ nullptr };
Gtk::MessageDialog* MainWindow::m_AskDeleteConfirmDialog{ nullptr };
Gtk::CheckButton* MainWindow::m_AskDeleteConfirmCheckButton{ nullptr };

MainWindow::MainWindow(BaseObjectType* cobj, Glib::RefPtr<Gtk::Builder> bldr)
    : Gtk::ApplicationWindow{ cobj },
      m_Builder{ std::move(bldr) },
      m_LastSavePath{ Settings.get_string("LastSavePath") }
{
    if (!m_PreferencesDialog)
        m_Builder->get_widget_derived("PreferencesDialog", m_PreferencesDialog);

    m_Builder->get_widget_derived("ThumbnailBar", m_ThumbnailBar);
    m_Builder->get_widget_derived("Booru::Browser", m_BooruBrowser);
    m_Builder->get_widget_derived("Booru::Browser::InfoBox", m_InfoBox);
    m_Builder->get_widget_derived("ImageBox", m_ImageBox);
    m_Builder->get_widget_derived("StatusBar", m_StatusBar);

    m_Builder->get_widget("MainWindow::HPaned", m_HPaned);
    m_HPaned->property_position().signal_changed().connect([&]() {
        if (!m_BooruBrowser->get_realized())
            return;

        auto half_window{ (get_width() - m_HPaned->get_handle_window()->get_width()) / 2 };
        if (m_HPaned->get_position() > half_window)
            m_HPaned->set_position(half_window);

        if (m_HPanedLastPos != m_HPaned->get_position())
        {
            m_HPanedLastPos = m_HPaned->get_position();
            Settings.set("BooruWidth", m_HPanedLastPos);
            m_ImageBox->queue_draw_image();
        }
    });
    m_Builder->get_widget_derived("Booru::Browser::TagView", m_TagView);

    m_UIManager = Glib::RefPtr<Gtk::UIManager>::cast_static(m_Builder->get_object("UIManager"));

    create_actions();

    g_signal_connect(this->gobj(), "screen-changed", G_CALLBACK(on_screen_changed), this);
    on_screen_changed(GTK_WIDGET(this->gobj()), nullptr, this);

    m_LocalImageList = std::make_shared<ImageList>(m_ThumbnailBar);
    m_LocalImageList->signal_archive_error().connect(
        [&](const std::string e) { m_StatusBar->set_message(e); });
    m_LocalImageList->signal_load_success().connect(
        [&]() { set_active_imagelist(m_LocalImageList); });
    m_LocalImageList->signal_size_changed().connect([&]() {
        if (m_LocalImageList == m_ActiveImageList)
        {
            update_title();
            set_sensitives();
        }
    });

    m_BooruBrowser->signal_page_changed().connect([&](Booru::Page* page) {
        set_active_imagelist(page ? page->get_imagelist() : m_LocalImageList);
    });
    m_BooruBrowser->signal_realize().connect(
        [&]() { m_HPaned->set_position(Settings.get_int("BooruWidth")); });
    m_BooruBrowser->signal_entry_blur().connect([&]() { m_ImageBox->grab_focus(); });

    m_ImageBox->signal_image_drawn().connect(sigc::mem_fun(*this, &MainWindow::update_title));
    m_ImageBox->signal_slideshow_ended().connect(
        sigc::mem_fun(*this, &MainWindow::on_toggle_slideshow));

    m_PreferencesDialog->signal_bg_color_set().connect(
        sigc::mem_fun(m_ImageBox, &ImageBox::update_background_color));
    m_PreferencesDialog->signal_title_format_changed().connect(
        sigc::mem_fun(*this, &MainWindow::update_title));
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
    if (!m_AboutDialog)
    {
        m_Builder->get_widget("AboutDialog", m_AboutDialog);
        m_AboutDialog->signal_response().connect([&](const int) { m_AboutDialog->hide(); });

        try
        {
#ifdef _WIN32
            std::string path;
            gchar* g{ g_win32_get_package_installation_directory_of_module(NULL) };
            if (g)
            {
                path = Glib::build_filename(
                    g, "share", "icons", "hicolor", "256x256", "apps", "ahoviewer.png");
                g_free(g);
            }
            Glib::RefPtr<Gdk::Pixbuf> logo{ Gdk::Pixbuf::create_from_file(path) };
            m_AboutDialog->set_logo(logo);
#else  // !_WIN32
            m_AboutDialog->set_logo_icon_name(PACKAGE);
#endif // !_WIN32
        }
        catch (...)
        {
        }

        m_AboutDialog->set_name(PACKAGE);
        m_AboutDialog->set_version(ahoviewer_version);
        m_AboutDialog->set_copyright(u8"Copyright \u00A9 2013-2021 ahoka");
        m_AboutDialog->set_website(PACKAGE_URL);
        m_AboutDialog->set_website_label(PACKAGE_URL);
        m_AboutDialog->add_credit_section(_("Created by"), { "ahoka" });
        m_AboutDialog->add_credit_section(_("Contributions by"),
                                          {
                                              "oliwer",
                                              "HaxtonFale",
                                              "WebFreak001",
                                              "theKlanc",
                                          });
    }
    // }}}

    // Used when deleting files and AskDeleteConfirm is true
    if (!m_AskDeleteConfirmDialog)
    {
        m_Builder->get_widget("AskDeleteConfirmDialog", m_AskDeleteConfirmDialog);
        m_AskDeleteConfirmDialog->signal_response().connect(
            [&](const int) { m_AskDeleteConfirmDialog->hide(); });

        m_Builder->get_widget("AskDeleteConfirmDialog::CheckButton", m_AskDeleteConfirmCheckButton);

        m_PreferencesDialog->signal_ask_delete_confirm_changed().connect(
            [&](bool v) { m_AskDeleteConfirmCheckButton->set_active(v); });
    }

    // Setup drag and drop
    drag_dest_set({ Gtk::TargetEntry("text/uri-list") },
                  Gtk::DEST_DEFAULT_ALL,
                  Gdk::ACTION_COPY | Gdk::ACTION_MOVE);

    // Call this to make sure it is rendered in the main thread.
    Image::get_missing_pixbuf();

    // Restore window geometry
    int x, y, w, h;
    if (Settings.get_geometry(x, y, w, h))
    {
        m_LastXPos = x;
        m_LastYPos = y;

        if (Settings.get_bool("RememberWindowSize"))
            set_default_size(w, h);

        set_position(Gtk::WIN_POS_NONE);
        show_all();

        if (Settings.get_bool("RememberWindowPos"))
            move(x, y);
    }
    else
    {
        auto dpy{ Gdk::Display::get_default() };
        int x, y;
        Gdk::Rectangle rect;

        dpy->get_default_seat()->get_pointer()->get_position(x, y);
        dpy->get_monitor_at_point(x, y)->get_workarea(rect);

        set_default_size(rect.get_width() * 0.75, rect.get_height() * 0.9);
        show_all();
    }

    if (Settings.get_bool("StartFullscreen"))
        m_ActionGroup->get_action("ToggleFullscreen")->activate();
}

MainWindow::~MainWindow()
{
    m_ActiveImageList.reset();
    m_LocalImageList.reset();

    delete m_ThumbnailBar;
    delete m_BooruBrowser;
    delete m_InfoBox;
    delete m_TagView;
    delete m_ImageBox;
    delete m_HPaned;

    delete m_MenuBar;
    delete m_StatusBar;
}

void MainWindow::open_file(const std::string& path, const int index, const bool restore)
{
    if (path.empty())
        return;

    std::string absolute_path;
    std::string error;

    if (Glib::path_is_absolute(path))
    {
        absolute_path = path;
    }
    else
    {
        try
        {
            absolute_path = Glib::filename_from_uri(path);
        }
        catch (Glib::ConvertError&)
        {
        }

        // From relative path
        if (absolute_path.empty())
            absolute_path = Glib::build_filename(Glib::get_current_dir(), path);
    }

    // Check if this image list is already loaded,
    // no point in reloading it since there are dirwatches setup
    // just change the current image in the list
    auto iter{ std::find_if(
        m_LocalImageList->begin(), m_LocalImageList->end(), [&absolute_path](const auto i) {
            return i->get_path() == absolute_path;
        }) };
    if (iter != m_LocalImageList->end())
    {
        m_LocalImageList->set_current(iter - m_LocalImageList->begin());
        set_active_imagelist(m_LocalImageList);
    }
    // Dont waste time re-extracting the archive just go to the first image
    else if (m_LocalImageList->from_archive() &&
             absolute_path == m_LocalImageList->get_archive().get_path())
    {
        m_LocalImageList->go_first();
        set_active_imagelist(m_LocalImageList);
    }
    else if (!m_LocalImageList->load(absolute_path, error, index))
    {
        std::string uri{ Glib::filename_to_uri(absolute_path) };

        if (Gtk::RecentManager::get_default()->has_item(uri))
            Gtk::RecentManager::get_default()->remove_item(uri);

        // Reset this here incase we failed to restore the last open file on
        // startup
        m_LocalImageList->set_scroll_position({ -1, -1, ZoomMode::AUTO_FIT });

        m_StatusBar->set_message(error);
        return;
    }

    if (Settings.get_bool("StoreRecentFiles"))
        Gtk::RecentManager::get_default()->add_item(Glib::filename_to_uri(absolute_path));

    auto tb_action{ Glib::RefPtr<Gtk::ToggleAction>::cast_static(
        m_ActionGroup->get_action("ToggleThumbnailBar")) };

    // Show the thumbnail bar when opening a new file
    if (!restore && !tb_action->get_active())
        tb_action->set_active(true);
    else
        update_widgets_visibility();

    present();
}

void MainWindow::restore_last_file()
{
    if (Settings.get_bool("RememberLastFile"))
    {
        std::string path{ Settings.get_string("LastOpenFile") };
        m_LocalImageList->set_scroll_position({
            static_cast<double>(Settings.get_int("ScrollPosH")),
            static_cast<double>(Settings.get_int("ScrollPosV")),
            Settings.get_zoom_mode(),
        });

        if (!path.empty())
            open_file(path, Settings.get_int("ArchiveIndex"), true);
    }
}

void MainWindow::get_drawable_area_size(int& w, int& h) const
{
    get_size(w, h);

    if (m_ThumbnailBar->get_visible())
        w -= m_ThumbnailBar->get_width();

    // +1 = the paned handle min-width
    if (m_BooruBrowser->get_visible())
        w -= m_HPaned->get_position() + 1;

    if (m_MenuBar->get_visible())
        h -= m_MenuBar->get_height();

    if (m_StatusBar->get_visible())
        h -= m_StatusBar->get_height();
}

void MainWindow::on_realize()
{
    Gtk::ApplicationWindow::on_realize();

    update_widgets_visibility();

    // Only need this to be true if the booru browser isnt shown on startup
    m_BooruBrowser->m_FirstShow = !m_BooruBrowser->get_visible();
}

void MainWindow::on_check_resize()
{
    int w{ get_allocation().get_width() }, h{ get_allocation().get_height() };

    // Make sure we really need to redraw
    if (w != m_Width || h != m_Height)
        m_ImageBox->queue_draw_image();

    m_Width  = w;
    m_Height = h;

    save_window_geometry();

    Gtk::ApplicationWindow::on_check_resize();
}

bool MainWindow::on_delete_event(GdkEventAny* e)
{
    bool r = Gtk::ApplicationWindow::on_delete_event(e);
    on_quit();

    return r;
}

bool MainWindow::on_window_state_event(GdkEventWindowState* e)
{
    m_IsMinimized = (e->changed_mask & Gdk::WindowState::WINDOW_STATE_ICONIFIED) &&
                    (e->new_window_state & Gdk::WindowState::WINDOW_STATE_ICONIFIED);

    return Gtk::ApplicationWindow::on_window_state_event(e);
}

void MainWindow::on_drag_data_received(const Glib::RefPtr<Gdk::DragContext>& ctx,
                                       int,
                                       int,
                                       const Gtk::SelectionData& data,
                                       guint,
                                       guint time)
{
    std::vector<Glib::ustring> uris{ data.get_uris() };

    if (uris.size())
    {
        std::string uri;
        // In case the URI is not using the file scheme
        try
        {
            uri = Glib::filename_from_uri(uris[0]);
        }
        catch (const Glib::ConvertError& ex)
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

bool MainWindow::on_key_press_event(GdkEventKey* e)
{
    // Hacky method to allow arrow key accelerators for the imagebox
    if (m_ImageBox->has_focus() && (e->keyval == GDK_KEY_Up || e->keyval == GDK_KEY_Down ||
                                    e->keyval == GDK_KEY_Left || e->keyval == GDK_KEY_Right))
    {
        GtkAccelGroupEntry* entries;
        if ((entries = gtk_accel_group_query(m_UIManager->get_accel_group()->gobj(),
                                             e->keyval,
                                             static_cast<GdkModifierType>(e->state),
                                             nullptr)))
        {
            std::string path{ g_quark_to_string(entries[0].accel_path_quark) };
            m_ActionGroup->get_action(path.substr(path.rfind('/') + 1))->activate();

            return true;
        }
    }
    else if (m_BooruBrowser->get_tag_entry()->has_focus() && (e->state & GDK_CONTROL_MASK) == 0)
    {
        return m_BooruBrowser->get_tag_entry()->event(reinterpret_cast<GdkEvent*>(e));
    }

    return Gtk::ApplicationWindow::on_key_press_event(e);
}

// Reveals the booru infobox when either the tagview, booru vpaned handle, or infobox itself is
// under the cursor
bool MainWindow::on_motion_notify_event(GdkEventMotion* e)
{
    if (!m_BooruBrowser->is_visible() || !Settings.get_bool("AutoHideInfoBox") ||
        !m_TagView->get_window() || !m_BooruBrowser->get_handle_window())
        return Gtk::ApplicationWindow::on_motion_notify_event(e);

    int x, y, w, h;
    Gdk::Rectangle cursor_rect{ static_cast<int>(e->x_root), static_cast<int>(e->y_root), 1, 1 };
    m_TagView->get_window()->get_origin(x, y);
    w = m_TagView->get_window()->get_width();
    h = m_TagView->get_window()->get_height();
    Gdk::Rectangle rect{ x, y, w, h };

    m_BooruBrowser->get_handle_window()->get_origin(x, y);
    w = m_BooruBrowser->get_handle_window()->get_width();
    h = m_BooruBrowser->get_handle_window()->get_height();
    rect.join({ x, y, w, h });

    if (m_InfoBox->get_window())
    {
        m_InfoBox->get_window()->get_origin(x, y);
        w = m_InfoBox->get_window()->get_width();
        h = m_InfoBox->get_window()->get_height();
        rect.join({ x, y, w, h });
    }

    if (rect.intersects(cursor_rect))
    {
        if (!m_InfoBox->is_visible())
            m_InfoBox->show();
    }
    else if (m_InfoBox->is_visible())
    {
        m_InfoBox->hide();
    }

    return Gtk::ApplicationWindow::on_motion_notify_event(e);
}

void MainWindow::set_active_imagelist(const std::shared_ptr<ImageList>& image_list)
{
    if (m_ActiveImageList == image_list)
        return;

    if (m_ActiveImageList && !m_ActiveImageList->empty())
        m_ActiveImageList->set_scroll_position(m_ImageBox->get_scroll_position());

    m_ImageListConn.disconnect();
    m_ImageListClearedConn.disconnect();
    m_ActiveImageList = image_list;

    m_ImageListConn = m_ActiveImageList->signal_changed().connect(
        sigc::mem_fun(*this, &MainWindow::on_imagelist_changed));
    m_ImageListClearedConn = m_ActiveImageList->signal_cleared().connect(
        sigc::mem_fun(*this, &MainWindow::on_imagelist_cleared));

    if (!m_ActiveImageList->empty())
    {
        m_ImageBox->clear_image();
        m_ImageBox->set_restore_scroll_position(m_ActiveImageList->get_scroll_position());
        on_imagelist_changed(m_ActiveImageList->get_current());
    }
    else
    {
        on_imagelist_cleared();
    }
}

// Called when before quitting and before fullscreening
void MainWindow::save_window_geometry()
{
    if (is_fullscreen())
        return;

    int x{ m_LastXPos }, y{ m_LastYPos }, w, h;

    get_size(w, h);
    // On Windows this will always return -32000, -32000 when minimized
    // XXX: Is this still the case with gtk3?
    if (!m_IsMinimized)
    {
        get_position(x, y);
        m_LastXPos = x;
        m_LastYPos = y;
    }

    Settings.set_geometry(x, y, w, h);
}

// Creates the UIManager and sets up all of the actions.
// Also adds the menubar to the table.
void MainWindow::create_actions()
{
    // Create the action group for the UIManager
    m_ActionGroup = Gtk::ActionGroup::create(PACKAGE);
    m_ActionGroup->set_accel_group(m_UIManager->get_accel_group());

    // Menu actions {{{
    m_ActionGroup->add(Gtk::Action::create("FileMenu", _("_File")));
    m_RecentAction = Gtk::RecentAction::create("RecentMenu",
                                               Gtk::Stock::DND_MULTIPLE,
                                               _("_Open Recent"),
                                               "",
                                               Gtk::RecentManager::get_default());
    m_RecentAction->signal_item_activated().connect(
        sigc::mem_fun(*this, &MainWindow::on_open_recent_file));
    m_ActionGroup->add(m_RecentAction);
    m_ActionGroup->add(Gtk::Action::create("ViewMenu", _("_View")));
    m_ActionGroup->add(Gtk::Action::create("ZoomMenu", _("_Zoom")));
    m_ActionGroup->add(Gtk::Action::create("GoMenu", _("_Go")));
    m_ActionGroup->add(Gtk::Action::create("HelpMenu", _("_Help")));
    // }}}

    // Normal actions {{{
    m_ActionGroup->add(
        Gtk::Action::create("OpenFile", Gtk::Stock::OPEN, _("_Open File"), _("Open a file")),
        Gtk::AccelKey(Settings.get_keybinding("File", "OpenFile")),
        sigc::mem_fun(*this, &MainWindow::on_open_file_dialog));
    m_ActionGroup->add(Gtk::Action::create_with_icon_name("Preferences",
                                                          "preferences-system",
                                                          _("_Preferences"),
                                                          _("Open the preferences dialog")),
                       Gtk::AccelKey(Settings.get_keybinding("File", "Preferences")),
                       sigc::mem_fun(*this, &MainWindow::on_show_preferences));
    m_ActionGroup->add(
        Gtk::Action::create(
            "Close", Gtk::Stock::CLOSE, _("_Close"), _("Close local image list or booru tab")),
        Gtk::AccelKey(Settings.get_keybinding("File", "Close")),
        sigc::mem_fun(*this, &MainWindow::on_close));
    m_ActionGroup->add(
        Gtk::Action::create("Quit", Gtk::Stock::QUIT, _("_Quit"), _("Quit " PACKAGE)),
        Gtk::AccelKey(Settings.get_keybinding("File", "Quit")),
        sigc::mem_fun(*this, &MainWindow::on_quit));

    m_ActionGroup->add(Gtk::Action::create("ZoomIn", Gtk::Stock::ZOOM_IN, _("Zoom _In")),
                       Gtk::AccelKey(Settings.get_keybinding("Zoom", "ZoomIn")),
                       sigc::mem_fun(m_ImageBox, &ImageBox::on_zoom_in));
    m_ActionGroup->add(Gtk::Action::create("ZoomOut", Gtk::Stock::ZOOM_OUT, _("Zoom _Out")),
                       Gtk::AccelKey(Settings.get_keybinding("Zoom", "ZoomOut")),
                       sigc::mem_fun(m_ImageBox, &ImageBox::on_zoom_out));
    m_ActionGroup->add(Gtk::Action::create("ResetZoom",
                                           Gtk::Stock::ZOOM_100,
                                           _("_Reset Zoom"),
                                           _("Reset image to it's original size")),
                       Gtk::AccelKey(Settings.get_keybinding("Zoom", "ResetZoom")),
                       sigc::mem_fun(m_ImageBox, &ImageBox::on_reset_zoom));

    m_ActionGroup->add(
        Gtk::Action::create(
            "NextImage", Gtk::Stock::GO_FORWARD, _("_Next Image"), _("Go to next image")),
        Gtk::AccelKey(Settings.get_keybinding("Navigation", "NextImage")),
        sigc::mem_fun(*this, &MainWindow::on_next_image));
    m_ActionGroup->add(
        Gtk::Action::create(
            "PreviousImage", Gtk::Stock::GO_BACK, _("_Previous Image"), _("Go to previous image")),
        Gtk::AccelKey(Settings.get_keybinding("Navigation", "PreviousImage")),
        sigc::mem_fun(*this, &MainWindow::on_previous_image));
    m_ActionGroup->add(
        Gtk::Action::create(
            "FirstImage", Gtk::Stock::GOTO_FIRST, _("_First Image"), _("Go to first image")),
        Gtk::AccelKey(Settings.get_keybinding("Navigation", "FirstImage")),
        sigc::mem_fun(*this, &MainWindow::on_first_image));
    m_ActionGroup->add(
        Gtk::Action::create(
            "LastImage", Gtk::Stock::GOTO_LAST, _("_Last Image"), _("Go to last image")),
        Gtk::AccelKey(Settings.get_keybinding("Navigation", "LastImage")),
        sigc::mem_fun(*this, &MainWindow::on_last_image));

    m_ActionGroup->add(Gtk::Action::create("ReportIssue",
                                           Gtk::Stock::DIALOG_WARNING,
                                           _("_Report Issue"),
                                           _("Report an issue on the issue tracker")),
                       []() { Gio::AppInfo::launch_default_for_uri(PACKAGE_BUGREPORT); });
    m_ActionGroup->add(
        Gtk::Action::create(
            "About", Gtk::Stock::ABOUT, _("_About"), _("View information about " PACKAGE)),
        sigc::mem_fun(*this, &MainWindow::on_show_about));

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
    m_ActionGroup->add(Gtk::Action::create("SaveImageAs",
                                           Gtk::Stock::SAVE_AS,
                                           _("Save Image as..."),
                                           _("Save the selected image with file chooser")),
                       Gtk::AccelKey(Settings.get_keybinding("BooruBrowser", "SaveImageAs")),
                       sigc::mem_fun(*this, &MainWindow::on_save_image_as));
    m_ActionGroup->add(
        Gtk::Action::create(
            "SaveImage", Gtk::Stock::SAVE, _("Save Image"), _("Save the selected image")),
        Gtk::AccelKey(Settings.get_keybinding("BooruBrowser", "SaveImage")),
        sigc::mem_fun(*this, &MainWindow::on_save_image));
    m_ActionGroup->add(Gtk::Action::create_with_icon_name(
                           "SaveImages", "document-save-all", _("Save Images"), _("Save Images")),
                       Gtk::AccelKey(Settings.get_keybinding("BooruBrowser", "SaveImages")),
                       sigc::mem_fun(m_BooruBrowser, &Booru::Browser::on_save_images));
    m_ActionGroup->add(
        Gtk::Action::create_with_icon_name(
            "DeleteImage", "edit-delete", _("Delete Image"), _("Delete the selected image")),
        Gtk::AccelKey(Settings.get_keybinding("File", "DeleteImage")),
        sigc::mem_fun(*this, &MainWindow::on_delete_image));

    m_ActionGroup->add(
        Gtk::Action::create("ViewPost",
                            Gtk::Stock::PROPERTIES,
                            _("View Post"),
                            _("View the selected image's post in your default web browser")),
        Gtk::AccelKey(Settings.get_keybinding("BooruBrowser", "ViewPost")),
        sigc::mem_fun(m_BooruBrowser, &Booru::Browser::on_view_post));
    m_ActionGroup->add(Gtk::Action::create("CopyImageURL",
                                           Gtk::Stock::COPY,
                                           _("Copy Image URL"),
                                           _("Copy the selected image's URL to your clipboard")),
                       Gtk::AccelKey(Settings.get_keybinding("BooruBrowser", "CopyImageURL")),
                       sigc::mem_fun(m_BooruBrowser, &Booru::Browser::on_copy_image_url));
    m_ActionGroup->add(Gtk::Action::create("CopyImageData",
                                           Gtk::Stock::COPY,
                                           _("Copy Image"),
                                           _("Copy the selected image to your clipboard")),
                       Gtk::AccelKey(Settings.get_keybinding("BooruBrowser", "CopyImageData")),
                       sigc::mem_fun(m_BooruBrowser, &Booru::Browser::on_copy_image_data));
    m_ActionGroup->add(
        Gtk::Action::create("CopyPostURL",
                            Gtk::Stock::COPY,
                            _("Copy Post URL"),
                            _("Copy the selected image's post URL to your clipboard")),
        Gtk::AccelKey(Settings.get_keybinding("BooruBrowser", "CopyPostURL")),
        sigc::mem_fun(m_BooruBrowser, &Booru::Browser::on_copy_post_url));
    // }}}

    // Toggle actions {{{
    Glib::RefPtr<Gtk::ToggleAction> toggle_action;
    toggle_action = Gtk::ToggleAction::create(
        "ToggleFullscreen", _("_Fullscreen"), _("Toggle fullscreen mode"));
    m_ActionGroup->add(toggle_action,
                       Gtk::AccelKey(Settings.get_keybinding("UserInterface", "ToggleFullscreen")),
                       sigc::mem_fun(*this, &MainWindow::on_toggle_fullscreen));

    toggle_action = Gtk::ToggleAction::create("ToggleMangaMode",
                                              _("_Manga Mode"),
                                              _("Toggle manga mode"),
                                              Settings.get_bool("MangaMode"));
    m_ActionGroup->add(toggle_action,
                       Gtk::AccelKey(Settings.get_keybinding("ViewMode", "ToggleMangaMode")),
                       sigc::mem_fun(*this, &MainWindow::on_toggle_manga_mode));

    toggle_action = Gtk::ToggleAction::create("ToggleMenuBar",
                                              _("_Menubar"),
                                              _("Toggle menubar visibility"),
                                              Settings.get_bool("MenuBarVisible"));
    m_ActionGroup->add(toggle_action,
                       Gtk::AccelKey(Settings.get_keybinding("UserInterface", "ToggleMenuBar")),
                       sigc::mem_fun(*this, &MainWindow::on_toggle_menu_bar));

    toggle_action = Gtk::ToggleAction::create("ToggleStatusBar",
                                              _("_Statusbar"),
                                              _("Toggle statusbar visibility"),
                                              Settings.get_bool("StatusBarVisible"));
    m_ActionGroup->add(toggle_action,
                       Gtk::AccelKey(Settings.get_keybinding("UserInterface", "ToggleStatusBar")),
                       sigc::mem_fun(*this, &MainWindow::on_toggle_status_bar));

    toggle_action = Gtk::ToggleAction::create("ToggleScrollbars",
                                              _("_Scrollbars"),
                                              _("Toggle scrollbar visibility"),
                                              Settings.get_bool("ScrollbarsVisible"));
    m_ActionGroup->add(toggle_action,
                       Gtk::AccelKey(Settings.get_keybinding("UserInterface", "ToggleScrollbars")),
                       sigc::mem_fun(*this, &MainWindow::on_toggle_scrollbars));

    toggle_action = Gtk::ToggleAction::create("ToggleBooruBrowser",
                                              _("_Booru Browser"),
                                              _("Toggle booru browser visibility"),
                                              Settings.get_bool("BooruBrowserVisible"));
    toggle_action->set_draw_as_radio(true);
    m_ActionGroup->add(
        toggle_action,
        Gtk::AccelKey(Settings.get_keybinding("UserInterface", "ToggleBooruBrowser")),
        sigc::mem_fun(*this, &MainWindow::on_toggle_booru_browser));

    toggle_action = Gtk::ToggleAction::create("ToggleThumbnailBar",
                                              _("_Thumbnail Bar"),
                                              _("Toggle thumbnailbar visibility"),
                                              Settings.get_bool("ThumbnailBarVisible"));
    toggle_action->set_draw_as_radio(true);
    m_ActionGroup->add(
        toggle_action,
        Gtk::AccelKey(Settings.get_keybinding("UserInterface", "ToggleThumbnailBar")),
        sigc::mem_fun(*this, &MainWindow::on_toggle_thumbnail_bar));

    toggle_action = Gtk::ToggleAction::create("ToggleHideAll",
                                              _("_Hide All"),
                                              _("Toggle visibility of all interface widgets"),
                                              Settings.get_bool("HideAll"));
    m_ActionGroup->add(toggle_action,
                       Gtk::AccelKey(Settings.get_keybinding("UserInterface", "ToggleHideAll")),
                       sigc::mem_fun(*this, &MainWindow::on_toggle_hide_all));

    toggle_action = Gtk::ToggleAction::create(
        "ToggleSlideshow", _("_Slideshow"), _("Start or stop the slideshow"));
    m_ActionGroup->add(toggle_action,
                       Gtk::AccelKey(Settings.get_keybinding("Navigation", "ToggleSlideshow")),
                       sigc::mem_fun(*this, &MainWindow::on_toggle_slideshow));

    toggle_action = Gtk::ToggleAction::create(
        "ShowTagTypeHeaders", _("Show Tag Type Headers"), _("Show headers for each tag type"));
    toggle_action->set_active(Settings.get_bool("ShowTagTypeHeaders"));
    m_ActionGroup->add(toggle_action,
                       Gtk::AccelKey(),
                       sigc::mem_fun(m_TagView, &Booru::TagView::on_toggle_show_headers));

    toggle_action = Gtk::ToggleAction::create(
        "AutoHideInfoBox", _("Auto Hide"), _("Auto Hide the Info Box when not hovered around it"));
    toggle_action->set_active(Settings.get_bool("AutoHideInfoBox"));
    m_ActionGroup->add(toggle_action,
                       Gtk::AccelKey(),
                       sigc::mem_fun(m_InfoBox, &Booru::InfoBox::on_toggle_auto_hide));
    // }}}

    // Radio actions {{{
    Gtk::RadioAction::Group zoom_mode_group, tag_view_group;
    Glib::RefPtr<Gtk::RadioAction> radio_action;

    radio_action =
        Gtk::RadioAction::create(zoom_mode_group,
                                 "AutoFitMode",
                                 _("_Auto Fit Mode"),
                                 _("Fit the image to either the height or width of the window"));
    radio_action->property_value().set_value(static_cast<int>(ZoomMode::AUTO_FIT));
    m_ActionGroup->add(
        radio_action,
        Gtk::AccelKey(Settings.get_keybinding("ViewMode", "AutoFitMode")),
        sigc::bind(sigc::mem_fun(m_ImageBox, &ImageBox::set_zoom_mode), ZoomMode::AUTO_FIT));
    radio_action = Gtk::RadioAction::create(zoom_mode_group,
                                            "FitWidthMode",
                                            _("Fit _Width Mode"),
                                            _("Fit the image to the width of the window"));
    radio_action->property_value().set_value(static_cast<int>(ZoomMode::FIT_WIDTH));
    m_ActionGroup->add(
        radio_action,
        Gtk::AccelKey(Settings.get_keybinding("ViewMode", "FitWidthMode")),
        sigc::bind(sigc::mem_fun(m_ImageBox, &ImageBox::set_zoom_mode), ZoomMode::FIT_WIDTH));
    radio_action = Gtk::RadioAction::create(zoom_mode_group,
                                            "FitHeightMode",
                                            _("Fit _Height Mode"),
                                            _("Fit the image to the height of the window"));
    radio_action->property_value().set_value(static_cast<int>(ZoomMode::FIT_HEIGHT));
    m_ActionGroup->add(
        radio_action,
        Gtk::AccelKey(Settings.get_keybinding("ViewMode", "FitHeightMode")),
        sigc::bind(sigc::mem_fun(m_ImageBox, &ImageBox::set_zoom_mode), ZoomMode::FIT_HEIGHT));
    radio_action = Gtk::RadioAction::create(
        zoom_mode_group,
        "ManualZoomMode",
        _("_Manual Zoom"),
        _("Display the image at it's original size, with the ability to zoom in and out"));
    radio_action->property_value().set_value(static_cast<int>(ZoomMode::MANUAL));
    m_ActionGroup->add(
        radio_action,
        Gtk::AccelKey(Settings.get_keybinding("ViewMode", "ManualZoomMode")),
        sigc::bind(sigc::mem_fun(m_ImageBox, &ImageBox::set_zoom_mode), ZoomMode::MANUAL));

    radio_action->set_current_value(static_cast<int>(m_ImageBox->get_zoom_mode()));

    radio_action = Gtk::RadioAction::create(
        tag_view_group, "SortByType", _("Sort by Type"), _("Sort booru tags by their type"));
    radio_action->property_value().set_value(static_cast<int>(Booru::TagViewOrder::TYPE));
    m_ActionGroup->add(radio_action,
                       Gtk::AccelKey(),
                       sigc::bind(sigc::mem_fun(m_TagView, &Booru::TagView::set_sort_order),
                                  Booru::TagViewOrder::TYPE));
    radio_action = Gtk::RadioAction::create(
        tag_view_group, "SortByTag", _("Sort by Tag"), _("Sort booru tags alphabetically"));
    radio_action->property_value().set_value(static_cast<int>(Booru::TagViewOrder::TAG));
    m_ActionGroup->add(radio_action,
                       Gtk::AccelKey(),
                       sigc::bind(sigc::mem_fun(m_TagView, &Booru::TagView::set_sort_order),
                                  Booru::TagViewOrder::TAG));

    radio_action->set_current_value(static_cast<int>(Settings.get_tag_view_order()));
    // }}}

    m_UIManager->insert_action_group(m_ActionGroup);

    // Setup tooltip handling
    m_UIManager->signal_connect_proxy().connect(
        sigc::mem_fun(*this, &MainWindow::on_connect_proxy));

    // Create the recent menu filter
    m_RecentAction->set_show_tips();
    m_RecentAction->set_sort_type(Gtk::RECENT_SORT_MRU);

    Glib::RefPtr<Gtk::RecentFilter> filter{ Gtk::RecentFilter::create() };
    filter->add_pixbuf_formats();

    for (const std::string& mime_type : Archive::MimeTypes)
        filter->add_mime_type(mime_type);

    for (const std::string& ext : Archive::FileExtensions)
        filter->add_pattern("*." + ext);

#ifdef HAVE_GSTREAMER
    filter->add_pattern("*.webm");
    filter->add_pattern("*.mp4");
    filter->add_mime_type("video/webm");
    filter->add_mime_type("video/mp4");
#endif // HAVE_GSTREAMER

    m_RecentAction->add_filter(filter);

    // Add the accel group to the window.
    add_accel_group(m_ActionGroup->get_accel_group());

    m_MenuBar = static_cast<Gtk::MenuBar*>(m_UIManager->get_widget("/MenuBar"));
    m_MenuBar->set_hexpand();

#ifdef HAVE_LIBPEAS
    auto plugins_menuitem{ Gtk::make_managed<Gtk::MenuItem>(_("Plugins")) };
    auto plugins_menu{ Gtk::make_managed<Gtk::Menu>() };
    plugins_menuitem->set_submenu(*plugins_menu);

    for (auto& p : Application::get_default()->get_plugin_manager().get_window_plugins())
    {
        if (p->get_action_name().empty())
            continue;

        auto action{ Gtk::Action::create(
            p->get_action_name(), p->get_name(), p->get_description()) };
        m_ActionGroup->add(action,
                           Gtk::AccelKey(Settings.get_keybinding("Plugins", p->get_action_name())),
                           sigc::bind(sigc::mem_fun(*p, &Plugin::WindowPlugin::on_activate), this));

        if (!p->is_hidden())
            plugins_menu->append(*action->create_menu_item());
    }

    // Only add the plugins menu item if there are plugin actions to show
    //   3 = after the Go menuitem
    if (plugins_menu->get_children().size() > 0)
        m_MenuBar->insert(*plugins_menuitem, 3);
#endif // HAVE_LIBPEAS

    // Finally pack the menubar into the main window's box
    Gtk::Box* box{ nullptr };
    m_Builder->get_widget("MainWindow::Box", box);
    box->pack_start(*m_MenuBar, false, true);
    // pack_start doesn't seem to put the menu bar at the top of the box, so it needs to be moved
    box->reorder_child(*m_MenuBar, 0);
}

void MainWindow::update_widgets_visibility()
{
    // Get all the visibility settings for this window instead of the global
    // Settings (which may have been modified by another window)
    bool hide_all = Glib::RefPtr<Gtk::ToggleAction>::cast_static(
                        m_ActionGroup->get_action("ToggleHideAll"))
                        ->get_active(),
         menu_bar_visible = Glib::RefPtr<Gtk::ToggleAction>::cast_static(
                                m_ActionGroup->get_action("ToggleMenuBar"))
                                ->get_active(),
         status_bar_visible = Glib::RefPtr<Gtk::ToggleAction>::cast_static(
                                  m_ActionGroup->get_action("ToggleStatusBar"))
                                  ->get_active(),
         scroll_bars_visible = Glib::RefPtr<Gtk::ToggleAction>::cast_static(
                                   m_ActionGroup->get_action("ToggleScrollbars"))
                                   ->get_active(),
         booru_browser_visible = Glib::RefPtr<Gtk::ToggleAction>::cast_static(
                                     m_ActionGroup->get_action("ToggleBooruBrowser"))
                                     ->get_active(),
         thumbnail_bar_visible = Glib::RefPtr<Gtk::ToggleAction>::cast_static(
                                     m_ActionGroup->get_action("ToggleThumbnailBar"))
                                     ->get_active();

    get_window()->freeze_updates();

    m_MenuBar->set_visible(!hide_all && menu_bar_visible);
    m_StatusBar->set_visible(!hide_all && status_bar_visible);
    // The scrollbars are independent of the hideall setting
    m_ImageBox->get_hscrollbar()->set_visible(scroll_bars_visible);
    m_ImageBox->get_vscrollbar()->set_visible(scroll_bars_visible);
    m_BooruBrowser->set_visible(!hide_all && booru_browser_visible);
    m_ThumbnailBar->set_visible(!hide_all && thumbnail_bar_visible && !booru_browser_visible &&
                                !m_LocalImageList->empty());

    while (Glib::MainContext::get_default()->pending())
        Glib::MainContext::get_default()->iteration(true);

    m_ImageBox->queue_draw_image();
    set_sensitives();

    get_window()->thaw_updates();
}

void MainWindow::set_sensitives()
{
    bool hide_all{ Glib::RefPtr<Gtk::ToggleAction>::cast_static(
                       m_ActionGroup->get_action("ToggleHideAll"))
                       ->get_active() };
    static constexpr std::array<std::string_view, 3> ui_names{
        "ToggleMenuBar",
        "ToggleStatusBar",
        "ToggleBooruBrowser",
    };

    for (const auto s : ui_names)
        m_ActionGroup->get_action(s.data())->set_sensitive(!hide_all);

    static constexpr std::array<std::string_view, 5> action_names{
        "NextImage", "PreviousImage", "FirstImage", "LastImage", "ToggleSlideshow",
    };

    for (const auto s : action_names)
    {
        bool sens{ m_ActiveImageList && !m_ActiveImageList->empty() };

        if (s == "NextImage")
            sens = sens && m_ActiveImageList->can_go_next();
        else if (s == "PreviousImage")
            sens = sens && m_ActiveImageList->can_go_previous();

        m_ActionGroup->get_action(s.data())->set_sensitive(sens);
    }

    const Booru::Page* page{ m_BooruBrowser->get_active_page() };
    bool local{ !m_LocalImageList->empty() && m_LocalImageList == m_ActiveImageList },
        booru{ page && page->get_imagelist() == m_ActiveImageList },
        save{ (booru && !page->get_imagelist()->empty()) ||
              (local && m_ActiveImageList->from_archive()) };

    m_ActionGroup->get_action("ToggleThumbnailBar")
        ->set_sensitive(!hide_all && !m_LocalImageList->empty());

    m_ActionGroup->get_action("Close")->set_sensitive(local || booru);
    m_ActionGroup->get_action("NewTab")->set_sensitive(m_BooruBrowser->get_visible());
    m_ActionGroup->get_action("SaveImage")->set_sensitive(save);
    m_ActionGroup->get_action("SaveImageAs")->set_sensitive(save);
    m_ActionGroup->get_action("SaveImages")
        ->set_sensitive(booru && !page->get_imagelist()->empty());
    m_ActionGroup->get_action("DeleteImage")
        ->set_sensitive(local && !m_ActiveImageList->from_archive());
    m_ActionGroup->get_action("ViewPost")->set_sensitive(booru && !page->get_imagelist()->empty());
    m_ActionGroup->get_action("CopyImageURL")
        ->set_sensitive(booru && !page->get_imagelist()->empty());
    m_ActionGroup->get_action("CopyPostURL")
        ->set_sensitive(booru && !page->get_imagelist()->empty());
    m_ActionGroup->get_action("CopyImageData")
        ->set_sensitive(booru && !page->get_imagelist()->empty());
}

void MainWindow::update_title()
{
    if (m_ActiveImageList && !m_ActiveImageList->empty())
    {
        std::string fmt{ Settings.get_string("TitleFormat") };
        std::ostringstream ss;

        for (auto i = fmt.begin(); i < fmt.end(); ++i)
        {
            if (*i != '%')
            {
                ss << *i;
            }
            else if (++i != fmt.end())
            {
                switch (*i)
                {
                case '%':
                    ss << *i;
                    break;
                case 'c':
                    ss << m_ActiveImageList->get_size();
                    break;
                case 'f':
                    ss << m_ActiveImageList->get_current()->get_filename();
                    break;
                case 'h':
                    ss << m_ImageBox->get_orig_height();
                    break;
                case 'i':
                    ss << m_ActiveImageList->get_index() + 1;
                    break;
                case 'p':
                    ss << PACKAGE;
                    break;
                case 's':
                    ss << std::setprecision(1) << std::fixed;
                    ss << m_ImageBox->get_scale();
                    break;
                case 'w':
                    ss << m_ImageBox->get_orig_width();
                    break;
                case 'z':
                    ss << static_cast<char>(m_ImageBox->get_zoom_mode());
                    break;
                default:
                    std::cerr << "Invalid format specifier %" << *i << std::endl;
                    break;
                }
            }
        }

        set_title(ss.str());
    }
    else
    {
        set_title(PACKAGE);
    }
}

void MainWindow::save_image_as()
{
    auto dialog{ Gtk::FileChooserNative::create(
        "Save Image As", *this, Gtk::FILE_CHOOSER_ACTION_SAVE) };
    dialog->set_modal();

    const auto image{ std::static_pointer_cast<Archive::Image>(m_ActiveImageList->get_current()) };

    if (!m_LastSavePath.empty())
        dialog->set_current_folder(m_LastSavePath);

    dialog->set_current_name(Glib::path_get_basename(image->get_filename()));

    if (dialog->run() == Gtk::RESPONSE_ACCEPT)
    {
        std::string path = dialog->get_filename();
        m_LastSavePath   = Glib::path_get_dirname(path);
        image->save(path);
    }
}

bool MainWindow::is_fullscreen() const
{
    return get_window() && (get_window()->get_state() & Gdk::WINDOW_STATE_FULLSCREEN) != 0;
}

void MainWindow::on_imagelist_changed(const std::shared_ptr<Image>& image)
{
    m_StatusBar->set_page_info(m_ActiveImageList->get_index() + 1, m_ActiveImageList->get_size());
    m_StatusBar->set_filename(image->get_filename());

    m_ImageBox->set_image(image);
    set_sensitives();
}

void MainWindow::on_imagelist_cleared()
{
    if (m_LocalImageList == m_ActiveImageList)
    {
        const Booru::Page* page{ m_BooruBrowser->get_active_page() };

        if (page && !page->get_imagelist()->empty())
        {
            Glib::RefPtr<Gtk::ToggleAction>::cast_static(
                m_ActionGroup->get_action("ToggleBooruBrowser"))
                ->set_active();
            set_active_imagelist(page->get_imagelist());

            return;
        }
    }

    m_ImageBox->clear_image();
    m_StatusBar->clear_page_info();
    m_StatusBar->clear_filename();

    update_title();
    update_widgets_visibility();
}

void MainWindow::on_cache_size_changed()
{
    m_LocalImageList->on_cache_size_changed();
    for (const Booru::Page* p : m_BooruBrowser->get_pages())
        p->get_imagelist()->on_cache_size_changed();
}

void MainWindow::on_accel_edited(const std::string& accel_path, const std::string& action_name)
{
    auto action{ m_ActionGroup->get_action(action_name) };
    action->set_accel_path(accel_path);
    action->connect_accelerator();
}

void MainWindow::on_connect_proxy(const Glib::RefPtr<Gtk::Action>& action, Gtk::Widget* w)
{
    if (dynamic_cast<Gtk::MenuItem*>(w) && !action->get_tooltip().empty())
    {
        static_cast<Gtk::MenuItem*>(w)->signal_select().connect(
            sigc::bind(sigc::mem_fun(m_StatusBar, &StatusBar::set_message),
                       action->get_tooltip(),
                       StatusBar::Priority::TOOLTIP,
                       0));
        static_cast<Gtk::MenuItem*>(w)->signal_deselect().connect(sigc::bind(
            sigc::mem_fun(m_StatusBar, &StatusBar::clear_message), StatusBar::Priority::TOOLTIP));
    }
}

void MainWindow::on_screen_changed(GtkWidget* w, GdkScreen*, gpointer userp)
{
    auto self{ static_cast<MainWindow*>(userp) };
    auto screen{ Gdk::Screen::get_default() };
    auto visual{ screen->get_rgba_visual() };

    if (!visual || !screen->is_composited() || !Settings.get_bool("UseRGBAVisual"))
    {
        visual                = screen->get_system_visual();
        self->m_HasRGBAVisual = false;
        MainWindow::m_PreferencesDialog->set_has_rgba_visual(false);
    }
    else
    {
        self->m_HasRGBAVisual = true;
        MainWindow::m_PreferencesDialog->set_has_rgba_visual(true);

        self->get_style_context()->remove_class("background");
        self->m_MenuBar->get_style_context()->add_class("background");
        self->m_StatusBar->get_style_context()->add_class("background");
        self->m_BooruBrowser->get_style_context()->add_class("background");
    }

    gtk_widget_set_visual(w, visual->gobj());
}

void MainWindow::on_open_file_dialog()
{
    auto dialog{ Gtk::FileChooserNative::create("Open", *this, Gtk::FILE_CHOOSER_ACTION_OPEN) };
    auto filter{ Gtk::FileFilter::create() }, image_filter{ Gtk::FileFilter::create() },
        archive_filter{ Gtk::FileFilter::create() };

    filter->set_name(_("All Files"));
    image_filter->set_name(_("All Images"));
    archive_filter->set_name(_("All Archives"));

    filter->add_pixbuf_formats();
    image_filter->add_pixbuf_formats();

#ifdef HAVE_GSTREAMER
    auto video_filter{ Gtk::FileFilter::create() };
    video_filter->set_name(_("All Videos"));

    filter->add_pattern("*.webm");
    filter->add_pattern("*.mp4");
    video_filter->add_pattern("*.webm");
    video_filter->add_pattern("*.mp4");

#ifndef _WIN32
    filter->add_mime_type("video/webm");
    filter->add_mime_type("video/mp4");
    video_filter->add_mime_type("video/webm");
    video_filter->add_mime_type("video/mp4");
#endif // !_WIN32
#endif // HAVE_GSTREAMER

#ifndef _WIN32
    for (const std::string& mime_type : Archive::MimeTypes)
    {
        filter->add_mime_type(mime_type);
        archive_filter->add_mime_type(mime_type);
    }
#endif // !_WIN32

    for (const std::string& ext : Archive::FileExtensions)
    {
        filter->add_pattern("*." + ext);
        archive_filter->add_pattern("*." + ext);
    }

    dialog->add_filter(filter);
    dialog->add_filter(image_filter);
#ifdef HAVE_GSTREAMER
    dialog->add_filter(video_filter);
#endif // HAVE_GSTREAMER
#if defined(HAVE_LIBZIP) || defined(HAVE_LIBUNRAR)
    dialog->add_filter(archive_filter);
#endif

    if (!m_LocalImageList->empty())
    {
        // Set the starting location to the current file
        std::string path{ m_LocalImageList->from_archive()
                              ? m_LocalImageList->get_archive().get_path()
                              : m_LocalImageList->get_current()->get_path() };
        dialog->set_current_folder(Glib::path_get_dirname(path));
    }

    if (dialog->run() == Gtk::RESPONSE_ACCEPT)
        open_file(dialog->get_filename());
}

void MainWindow::on_show_preferences()
{
    auto w{ m_PreferencesDialog->get_transient_for() };

    if (m_PreferencesDialog->is_visible() && w == this)
        return;
    else if (m_PreferencesDialog->is_visible())
        m_PreferencesDialog->hide();

    m_PreferencesDialog->set_transient_for(*this);
    m_PreferencesDialog->show_all();
}

void MainWindow::on_show_about()
{
    auto w{ m_AboutDialog->get_transient_for() };

    if (m_AboutDialog->is_visible() && w == this)
        return;
    else if (m_AboutDialog->is_visible())
        m_AboutDialog->hide();

    m_AboutDialog->set_transient_for(*this);
    m_AboutDialog->show();
}

void MainWindow::on_open_recent_file()
{
    std::string uri{ m_RecentAction->get_current_uri() };

    if (!uri.empty())
        open_file(Glib::filename_from_uri(uri));
}

void MainWindow::on_close()
{
    if (m_LocalImageList == m_ActiveImageList)
        m_LocalImageList->clear();
    else
        m_BooruBrowser->on_close_tab();
}

void MainWindow::on_quit()
{
    if (m_BooruBrowser->get_realized())
    {
        Settings.set("BooruWidth", m_HPaned->get_position());
        Settings.set("SelectedBooru", m_BooruBrowser->get_selected_booru());
    }

    if (Settings.get_bool("RememberLastFile") && !m_LocalImageList->empty())
    {
        std::string path{ m_LocalImageList->get_current()->get_path() };

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
        auto scroll_pos{ m_LocalImageList == m_ActiveImageList
                             ? m_ImageBox->get_scroll_position()
                             : m_LocalImageList->get_scroll_position() };
        Settings.set("ScrollPosH", static_cast<int>(scroll_pos.h));
        Settings.set("ScrollPosV", static_cast<int>(scroll_pos.v));
    }
    else
    {
        Settings.remove("ArchiveIndex");
        Settings.remove("LastOpenFile");
        Settings.remove("ScrollPosH");
        Settings.remove("ScrollPosV");
    }

    // ActionName => SettingKey
    std::map<std::string, std::string> widget_vis{ {
        { "ToggleHideAll", "HideAll" },
        { "ToggleMenuBar", "MenuBarVisible" },
        { "ToggleStatusBar", "StatusBarVisible" },
        { "ToggleScrollbars", "ScrollbarsVisible" },
        { "ToggleBooruBrowser", "BooruBrowserVisible" },
        { "ToggleThumbnailBar", "ThumbnailBarVisible" },
    } };

    for (auto& w : widget_vis)
    {
        bool v{ Glib::RefPtr<Gtk::ToggleAction>::cast_static(m_ActionGroup->get_action(w.first))
                    ->get_active() };
        Settings.set(w.second, v);
    }

    if (Settings.get_bool("RememberLastSavePath"))
    {
        Settings.set("LastSavePath", m_BooruBrowser->get_last_save_path());
        Settings.set("LastLocalSavePath", m_LastSavePath);
    }
    else
    {
        Settings.remove("LastSavePath");
        Settings.remove("LastLocalSavePath");
    }

    save_window_geometry();
    hide();
}

void MainWindow::on_toggle_fullscreen()
{
    if (is_fullscreen())
    {
        if (Settings.get_bool("HideAllFullscreen") && m_HideAllFullscreen && !m_WasHideAll)
        {
            m_HideAllFullscreen = false;
            Glib::RefPtr<Gtk::ToggleAction>::cast_static(m_ActionGroup->get_action("ToggleHideAll"))
                ->set_active(false);
        }

        unfullscreen();
    }
    else
    {
        if (Settings.get_bool("HideAllFullscreen"))
        {
            m_HideAllFullscreen = true;
            auto a{ Glib::RefPtr<Gtk::ToggleAction>::cast_static(
                m_ActionGroup->get_action("ToggleHideAll")) };
            m_WasHideAll = a->get_active();
            a->set_active();
        }

        // Save this here in case the program is closed when fullscreen
        save_window_geometry();
        fullscreen();
    }
}

void MainWindow::on_toggle_manga_mode()
{
    bool active{ Glib::RefPtr<Gtk::ToggleAction>::cast_static(
                     m_ActionGroup->get_action("ToggleMangaMode"))
                     ->get_active() };

    Settings.set("MangaMode", active);

    m_ImageBox->queue_draw_image(true);
}

void MainWindow::on_toggle_menu_bar()
{
    auto a{ Glib::RefPtr<Gtk::ToggleAction>::cast_static(
        m_ActionGroup->get_action("ToggleMenuBar")) };

    Settings.set("MenuBarVisible", a->get_active());

    update_widgets_visibility();
}

void MainWindow::on_toggle_status_bar()
{
    auto a{ Glib::RefPtr<Gtk::ToggleAction>::cast_static(
        m_ActionGroup->get_action("ToggleStatusBar")) };

    Settings.set("StatusBarVisible", a->get_active());

    update_widgets_visibility();
}

void MainWindow::on_toggle_scrollbars()
{
    auto a{ Glib::RefPtr<Gtk::ToggleAction>::cast_static(
        m_ActionGroup->get_action("ToggleScrollbars")) };
    Settings.set("ScrollbarsVisible", a->get_active());

    update_widgets_visibility();
}

void MainWindow::on_toggle_booru_browser()
{
    auto tb_action{ Glib::RefPtr<Gtk::ToggleAction>::cast_static(
        m_ActionGroup->get_action("ToggleThumbnailBar")) };
    auto bb_action{ Glib::RefPtr<Gtk::ToggleAction>::cast_static(
        m_ActionGroup->get_action("ToggleBooruBrowser")) };

    Settings.set("BooruBrowserVisible", bb_action->get_active());

    if (bb_action->get_active())
    {
        bool hide_all{ Glib::RefPtr<Gtk::ToggleAction>::cast_static(
                           m_ActionGroup->get_action("ToggleHideAll"))
                           ->get_active() };

        if (!hide_all)
            m_BooruBrowser->get_tag_entry()->grab_focus();

        if (m_BooruBrowser->get_active_page() &&
            m_BooruBrowser->get_active_page()->get_imagelist() != m_ActiveImageList)
            set_active_imagelist(m_BooruBrowser->get_active_page()->get_imagelist());

        if (tb_action->get_active())
        {
            tb_action->set_active(false);
            return;
        }
    }

    update_widgets_visibility();
}

void MainWindow::on_toggle_thumbnail_bar()
{
    auto tb_action{ Glib::RefPtr<Gtk::ToggleAction>::cast_static(
        m_ActionGroup->get_action("ToggleThumbnailBar")) };
    auto bb_action{ Glib::RefPtr<Gtk::ToggleAction>::cast_static(
        m_ActionGroup->get_action("ToggleBooruBrowser")) };

    Settings.set("ThumbnailBarVisible", tb_action->get_active());

    if (tb_action->get_active())
    {
        if (m_ActiveImageList != m_LocalImageList)
            set_active_imagelist(m_LocalImageList);

        if (bb_action->get_active())
        {
            bb_action->set_active(false);
            return;
        }
    }

    update_widgets_visibility();
}

void MainWindow::on_toggle_hide_all()
{
    auto a{ Glib::RefPtr<Gtk::ToggleAction>::cast_static(
        m_ActionGroup->get_action("ToggleHideAll")) };

    // User toggled hide all after program set it to true
    if (m_HideAllFullscreen && !a->get_active())
        m_HideAllFullscreen = false;

    if (!m_HideAllFullscreen)
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
    if (m_ImageBox->is_slideshow_running())
        m_StatusBar->set_message(_("Slideshow started"));
    else
        m_StatusBar->set_message(_("Slideshow stopped"));
}

void MainWindow::on_save_image()
{
    const Booru::Page* page{ m_BooruBrowser->get_active_page() };
    bool archive{ !m_LocalImageList->empty() && m_LocalImageList == m_ActiveImageList &&
                  m_LocalImageList->from_archive() },
        booru{ page && page->get_imagelist() == m_ActiveImageList };

    if (booru)
    {
        m_BooruBrowser->on_save_image();
    }
    else if (archive)
    {
        if (!m_LastSavePath.empty())
        {
            const auto image{ std::static_pointer_cast<Archive::Image>(
                m_ActiveImageList->get_current()) };

            image->save(m_LastSavePath + "/" + Glib::path_get_basename(image->get_filename()));

            m_StatusBar->set_message(_("Saving image to ") + m_LastSavePath);
        }
        else
        {
            save_image_as();
        }
    }
}

void MainWindow::on_save_image_as()
{
    const Booru::Page* page{ m_BooruBrowser->get_active_page() };
    bool archive{ !m_LocalImageList->empty() && m_LocalImageList == m_ActiveImageList &&
                  m_LocalImageList->from_archive() },
        booru{ page && page->get_imagelist() == m_ActiveImageList };

    if (booru)
    {
        m_BooruBrowser->on_save_image_as();
    }
    else if (archive)
    {
        save_image_as();
    }
}

void MainWindow::on_delete_image()
{
    if (Settings.get_bool("AskDeleteConfirm"))
    {
        auto text{ Glib::ustring::compose(
            _("Are you sure that you want to delete '%1'?"),
            Glib::path_get_basename(m_ActiveImageList->get_current()->get_filename())) };

        m_AskDeleteConfirmDialog->set_message(text);
        m_AskDeleteConfirmDialog->property_transient_for() =
            static_cast<Gtk::Window*>(get_toplevel());
        // Set the default focus to the yes button
        m_AskDeleteConfirmDialog->get_widget_for_response(Gtk::RESPONSE_YES)->grab_focus();

        if (m_AskDeleteConfirmDialog->run() != Gtk::RESPONSE_YES)
            return;

        Settings.set("AskDeleteConfirm", m_AskDeleteConfirmCheckButton->get_active());
        m_PreferencesDialog->set_ask_delete_confirm(Settings.get_bool("AskDeleteConfirm"));
    }

    m_ActiveImageList->get_current()->trash();
}
