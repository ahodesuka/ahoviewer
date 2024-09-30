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
#include "thumbnailbar.h"

#include <glibmm/i18n.h>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <utility>

#ifdef GDK_WINDOWING_WIN32
#include <gdk/gdkwin32.h>
#endif // GDK_WINDOWING_WIN32

MainWindow::MainWindow(BaseObjectType* cobj, const Glib::RefPtr<Gtk::Builder>& bldr)
    : Gtk::ApplicationWindow{ cobj },
      m_LastSavePath{ Settings.get_string("LastSavePath") }
{
    try
    {
        bldr->add_from_resource("/ui/menus.ui");
    }
    catch (const Glib::Error& ex)
    {
        std::cerr << "Gtk::Builder::add_from_resource: " << ex.what() << std::endl;
    }

    bldr->get_widget("MainWindow::Box", m_Box);
    bldr->get_widget("MainWindow::Stack", m_Stack);
    bldr->get_widget_derived("ThumbnailBar", m_ThumbnailBar);
    bldr->get_widget_derived("Booru::Browser", m_BooruBrowser);
    bldr->get_widget_derived("Booru::Browser::TagEntry", m_TagEntry);
    bldr->get_widget_derived("Booru::Browser::TagView", m_TagView);
    bldr->get_widget_derived("Booru::Browser::InfoBox", m_InfoBox);
    bldr->get_widget_derived("ImageBox", m_ImageBox);
    bldr->get_widget_derived("StatusBar", m_StatusBar);

    bldr->get_widget("MainWindow::HPaned", m_HPaned);
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

    add_actions();

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
        sigc::bind(sigc::mem_fun(*this, &MainWindow::on_toggle_slideshow), false));

    auto prefs{ Application::get_default()->get_preferences_dialog() };
    prefs->signal_bg_color_set().connect(
        sigc::mem_fun(m_ImageBox, &ImageBox::update_background_color));
    prefs->signal_title_format_changed().connect(sigc::mem_fun(*this, &MainWindow::update_title));
    prefs->signal_cursor_hide_delay_changed().connect(
        sigc::mem_fun(m_ImageBox, &ImageBox::cursor_timeout));
    prefs->signal_cache_size_changed().connect(
        sigc::mem_fun(*this, &MainWindow::on_cache_size_changed));
    prefs->signal_slideshow_delay_changed().connect(
        sigc::mem_fun(m_ImageBox, &ImageBox::reset_slideshow));
    prefs->get_site_editor()->signal_edited().connect(
        sigc::mem_fun(m_BooruBrowser, &Booru::Browser::update_combobox_model));

    // Used when deleting files and AskDeleteConfirm is true
    bldr->get_widget("AskDeleteConfirmDialog", m_AskDeleteConfirmDialog);
    m_AskDeleteConfirmDialog->signal_response().connect(
        [&](const int) { m_AskDeleteConfirmDialog->hide(); });

    bldr->get_widget("AskDeleteConfirmDialog::CheckButton", m_AskDeleteConfirmCheckButton);

    prefs->signal_ask_delete_confirm_changed().connect(
        [&](bool v) { m_AskDeleteConfirmCheckButton->set_active(v); });

    // Setup drag and drop
    drag_dest_set({ Gtk::TargetEntry("text/uri-list") },
                  Gtk::DEST_DEFAULT_ALL,
                  Gdk::ACTION_COPY | Gdk::ACTION_MOVE);

    // Call this to make sure it is rendered in the main thread.
    Image::get_missing_pixbuf();
}

MainWindow::~MainWindow()
{
    // Make sure the booru browser removes and deletes it's notebook before it destroys
    // it's other child widgets
    delete m_BooruBrowser;
}

void MainWindow::show()
{
    // Restore window geometry
    int x, y, w, h;
    if (Settings.get_geometry(x, y, w, h))
    {
        m_LastXPos = x;
        m_LastYPos = y;

        if (Settings.get_bool("RememberWindowSize"))
            set_default_size(w, h);

        if (Settings.get_bool("RememberWindowPos"))
        {
            set_position(Gtk::WIN_POS_NONE);
            // Window must be shown before moving it
            show_all();

            move(x, y);
        }
    }

    if (!Settings.get_bool("RememberWindowSize"))
    {
        auto dpy{ Gdk::Display::get_default() };
        int x, y;
        Gdk::Rectangle rect;

        dpy->get_default_seat()->get_pointer()->get_position(x, y);
        dpy->get_monitor_at_point(x, y)->get_workarea(rect);

        set_default_size(rect.get_width() * 0.75, rect.get_height() * 0.9);
    }

    if (!Settings.get_bool("RememberWindowPos"))
    {
        set_position(Gtk::WIN_POS_CENTER);
        show_all();
    }
}

void MainWindow::open_file(const Glib::ustring& path, const int index, const bool restore)
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

    std::string uri{ Glib::filename_to_uri(absolute_path) };

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
        if (Gtk::RecentManager::get_default()->has_item(uri))
            Gtk::RecentManager::get_default()->remove_item(uri);

        // Reset this here incase we failed to restore the last open file on
        // startup
        m_LocalImageList->set_scroll_position({ -1, -1, ZoomMode::AUTO_FIT });

        m_StatusBar->set_message(error);
        return;
    }

    if (Settings.get_bool("StoreRecentFiles"))
        Gtk::RecentManager::get_default()->add_item(uri);

    // Show the thumbnail bar when opening a new file
    if (!restore)
    {
        lookup_action("ToggleThumbnailBar")->change_state(true);
        on_toggle_thumbnail_bar(true);
    }
    else
    {
        update_widgets_visibility();
    }

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
            open_file(Glib::filename_to_uri(path), Settings.get_int("ArchiveIndex"), true);
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

int MainWindow::get_menubar_height() const
{
    if (m_MenuBar->get_visible())
        return m_MenuBar->get_height();

    return 0;
}

// Prioritize key press events on the tag entry when it's focused.
// Without this window actions would take priority
bool MainWindow::on_key_press_event(GdkEventKey* e)
{
    bool ret{ false };
    if (m_TagEntry->is_focus())
        ret = m_TagEntry->on_key_press_event(e);

    return ret || Gtk::ApplicationWindow::on_key_press_event(e);
}

void MainWindow::on_realize()
{
    Gtk::ApplicationWindow::on_realize();

    m_MenuBar = Gtk::make_managed<Gtk::MenuBar>(get_application()->get_menubar());
    m_Box->pack_start(*m_MenuBar, false, false);
    m_Box->reorder_child(*m_MenuBar, 0);

    update_widgets_visibility();

    // Only need this to be true if the booru browser isnt shown on startup
    m_BooruBrowser->m_FirstShow = !m_BooruBrowser->get_visible();
}

void MainWindow::on_size_allocate(Gtk::Allocation& alloc)
{
    Gtk::ApplicationWindow::on_size_allocate(alloc);

    int w{ alloc.get_width() }, h{ alloc.get_height() };

    // Make sure we really need to redraw
    if (w != m_Width || h != m_Height)
        m_ImageBox->queue_draw_image();

    m_Width  = w;
    m_Height = h;

    save_window_geometry();

    m_LastSizeAllocateTime = std::chrono::steady_clock::now();
}

bool MainWindow::on_delete_event(GdkEventAny* e)
{
    bool r{ Gtk::ApplicationWindow::on_delete_event(e) };
    quit();

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
        open_file(uris[0]);
        ctx->drag_finish(true, false, time);
    }
    else
    {
        ctx->drag_finish(false, false, time);
    }
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

void MainWindow::add_actions()
{
    // This prevents having to store the action in the class which has the callback
    // Also handles the actual toggling of the state
    auto create_toggle_action = [&](const std::string& name, auto cb, bool state = false) {
        add_action_bool(
            name,
            [this, cb, name]() {
                bool state{ false };
                auto action{ lookup_action(name) };

                // Toggle actions do not change their states automatically
                action->get_state(state);
                state = !state;
                action->change_state(state);

                cb(state);
            },
            state);
    };

    add_action("OpenFile", sigc::mem_fun(*this, &MainWindow::on_open_file_dialog));
    add_action_with_parameter("OpenRecent",
                              Glib::VariantType(Glib::VARIANT_TYPE_STRING),
                              sigc::mem_fun(*this, &MainWindow::on_open_recent));
    add_action("Close", sigc::mem_fun(*this, &MainWindow::on_close));

    add_action("SaveImage", sigc::mem_fun(*this, &MainWindow::on_save_image));
    add_action("SaveImageAs", sigc::mem_fun(*this, &MainWindow::on_save_image_as));
    add_action("DeleteImage", sigc::mem_fun(*this, &MainWindow::on_delete_image));

    add_action("NextImage", sigc::mem_fun(*this, &MainWindow::on_next_image));
    add_action("PreviousImage", sigc::mem_fun(*this, &MainWindow::on_previous_image));
    add_action("FirstImage", sigc::mem_fun(*this, &MainWindow::on_first_image));
    add_action("LastImage", sigc::mem_fun(*this, &MainWindow::on_last_image));

    add_action("ZoomIn", sigc::mem_fun(m_ImageBox, &ImageBox::on_zoom_in));
    add_action("ZoomOut", sigc::mem_fun(m_ImageBox, &ImageBox::on_zoom_out));
    add_action("ResetZoom", sigc::mem_fun(m_ImageBox, &ImageBox::on_reset_zoom));

    add_action("ScrollUp", sigc::mem_fun(m_ImageBox, &ImageBox::on_scroll_up));
    add_action("ScrollDown", sigc::mem_fun(m_ImageBox, &ImageBox::on_scroll_down));
    add_action("ScrollLeft", sigc::mem_fun(m_ImageBox, &ImageBox::on_scroll_left));
    add_action("ScrollRight", sigc::mem_fun(m_ImageBox, &ImageBox::on_scroll_right));

    add_action("NewTab", sigc::mem_fun(m_BooruBrowser, &Booru::Browser::on_new_tab));
    add_action("SaveImages", sigc::mem_fun(m_BooruBrowser, &Booru::Browser::on_save_images));
    add_action("ViewPost", sigc::mem_fun(m_BooruBrowser, &Booru::Browser::on_view_post));
    add_action("CopyImageURL", sigc::mem_fun(m_BooruBrowser, &Booru::Browser::on_copy_image_url));
    add_action("CopyImageData", sigc::mem_fun(m_BooruBrowser, &Booru::Browser::on_copy_image_data));
    add_action("CopyPostURL", sigc::mem_fun(m_BooruBrowser, &Booru::Browser::on_copy_post_url));

    create_toggle_action("ToggleFullscreen",
                         sigc::mem_fun(*this, &MainWindow::on_toggle_fullscreen));
    create_toggle_action("ToggleMangaMode",
                         sigc::mem_fun(*this, &MainWindow::on_toggle_manga_mode),
                         Settings.get_bool("MangaMode"));
    create_toggle_action("ToggleMenuBar",
                         sigc::mem_fun(*this, &MainWindow::on_toggle_menu_bar),
                         Settings.get_bool("MenuBarVisible"));
    create_toggle_action("ToggleStatusBar",
                         sigc::mem_fun(*this, &MainWindow::on_toggle_status_bar),
                         Settings.get_bool("StatusBarVisible"));
    create_toggle_action("ToggleScrollbars",
                         sigc::mem_fun(*this, &MainWindow::on_toggle_scrollbars),
                         Settings.get_bool("ScrollbarsVisible"));
    create_toggle_action("ToggleThumbnailBar",
                         sigc::mem_fun(*this, &MainWindow::on_toggle_thumbnail_bar),
                         Settings.get_bool("ThumbnailBarVisible"));
    create_toggle_action("ToggleBooruBrowser",
                         sigc::mem_fun(*this, &MainWindow::on_toggle_booru_browser),
                         Settings.get_bool("BooruBrowserVisible"));
    create_toggle_action("ToggleHideAll",
                         sigc::mem_fun(*this, &MainWindow::on_toggle_hide_all),
                         Settings.get_bool("HideAll"));
    create_toggle_action("ToggleSlideshow", sigc::mem_fun(*this, &MainWindow::on_toggle_slideshow));
    create_toggle_action("ShowTagTypeHeaders",
                         sigc::mem_fun(*m_TagView, &Booru::TagView::on_toggle_show_headers),
                         Settings.get_bool("ShowTagTypeHeaders"));
    create_toggle_action("AutoHideInfoBox",
                         sigc::mem_fun(*this, &MainWindow::on_toggle_auto_hide_info_box),
                         Settings.get_bool("AutoHideInfoBox"));

    auto zoom_mode_string{ std::string{ static_cast<char>(Settings.get_zoom_mode()) } };
    add_action_radio_string(
        "ZoomMode",
        [&](const Glib::ustring& s) {
            lookup_action("ZoomMode")->change_state(s);
            auto zoom_mode{ static_cast<ZoomMode>(s[0]) };
            m_ImageBox->set_zoom_mode(zoom_mode);
            set_sensitives();
        },
        zoom_mode_string);

    auto tag_order_int{ static_cast<int>(Settings.get_tag_view_order()) };
    add_action_radio_integer(
        "TagViewSortBy",
        [&](int i) {
            lookup_action("TagViewSortBy")->change_state(i);
            auto order{ static_cast<Booru::TagViewOrder>(i) };
            m_TagView->set_sort_order(order);
        },
        tag_order_int);

#ifdef HAVE_LIBPEAS
    // window Plugin actions
    for (auto& p : Application::get_default()->get_plugin_manager().get_window_plugins())
    {
        if (p->get_action_name().empty())
            continue;

        add_action(p->get_action_name(),
                   sigc::bind(sigc::mem_fun(*p, &Plugin::WindowPlugin::on_activate), this));
    }
#endif // HAVE_LIBPEAS
}

void MainWindow::update_widgets_visibility()
{
    // Get all the visibility settings for this window instead of the global
    // Settings (which may have been modified by another window)
    bool hide_all{ false }, menu_bar_visible{ false }, status_bar_visible{ false },
        scroll_bars_visible{ false }, booru_browser_visible{ false },
        thumbnail_bar_visible{ false };

    auto action{ lookup_action("ToggleHideAll") };
    action->get_state(hide_all);
    lookup_action("ToggleMenuBar")->get_state(menu_bar_visible);
    lookup_action("ToggleStatusBar")->get_state(status_bar_visible);
    lookup_action("ToggleScrollbars")->get_state(scroll_bars_visible);
    lookup_action("ToggleBooruBrowser")->get_state(booru_browser_visible);
    lookup_action("ToggleThumbnailBar")->get_state(thumbnail_bar_visible);

    get_window()->freeze_updates();

    m_MenuBar->set_visible(!hide_all && menu_bar_visible);
    m_StatusBar->set_visible(!hide_all && status_bar_visible);
    // The scrollbars are independent of the hideall setting
    m_ImageBox->get_hscrollbar()->set_visible(scroll_bars_visible);
    m_ImageBox->get_vscrollbar()->set_visible(scroll_bars_visible);
    m_BooruBrowser->set_visible(!hide_all && booru_browser_visible);
    m_ThumbnailBar->set_visible(!hide_all && thumbnail_bar_visible && !booru_browser_visible &&
                                !m_LocalImageList->empty());

    m_ImageBox->queue_draw_image();
    set_sensitives();

    get_window()->thaw_updates();
}

void MainWindow::set_sensitives()
{
    const auto lookup_simple_action = [&](const std::string_view name) {
        return Glib::RefPtr<Gio::SimpleAction>::cast_dynamic(lookup_action(name.data()));
    };
    bool hide_all{ false };
    lookup_action("ToggleHideAll")->get_state(hide_all);

    // Thumbnailbar is handled later below, and scrollbars can be toggled independently of hide all
    static constexpr std::array<std::string_view, 3> ui_names{
        "ToggleMenuBar",
        "ToggleStatusBar",
        "ToggleBooruBrowser",
    };

    for (const auto s : ui_names)
        lookup_simple_action(s)->set_enabled(!hide_all);

    static constexpr std::array<std::string_view, 5> action_names{
        "NextImage", "PreviousImage", "FirstImage", "LastImage", "ToggleSlideshow",
    };

    bool image{ m_ActiveImageList && !m_ActiveImageList->empty() };
    for (const auto s : action_names)
    {
        bool sens{ image };
        if (s == "NextImage" || s == "LastImage")
            sens = image && m_ActiveImageList->can_go_next();
        else if (s == "PreviousImage" || s == "FirstImage")
            sens = image && m_ActiveImageList->can_go_previous();

        lookup_simple_action(s)->set_enabled(sens);
    }

    const Booru::Page* page{ m_BooruBrowser->get_active_page() };
    bool local{ !m_LocalImageList->empty() && m_LocalImageList == m_ActiveImageList },
        booru{ page && page->get_imagelist() == m_ActiveImageList },
        save{ (booru && !page->get_imagelist()->empty()) ||
              (local && m_ActiveImageList->from_archive()) },
        manual_zoom{ m_ImageBox->get_zoom_mode() == ZoomMode::MANUAL };

    lookup_simple_action("ToggleThumbnailBar")
        ->set_enabled(!hide_all && !m_LocalImageList->empty());

    lookup_simple_action("NewTab")->set_enabled(m_BooruBrowser->get_visible());
    lookup_simple_action("SaveImage")->set_enabled(save);
    lookup_simple_action("SaveImageAs")->set_enabled(save);
    lookup_simple_action("SaveImages")->set_enabled(booru && !page->get_imagelist()->empty());
    lookup_simple_action("DeleteImage")->set_enabled(local && !m_ActiveImageList->from_archive());
    lookup_simple_action("ViewPost")->set_enabled(booru && !page->get_imagelist()->empty());
    lookup_simple_action("CopyImageURL")->set_enabled(booru && !page->get_imagelist()->empty());
    lookup_simple_action("CopyPostURL")->set_enabled(booru && !page->get_imagelist()->empty());
    lookup_simple_action("CopyImageData")->set_enabled(booru && !page->get_imagelist()->empty());
    lookup_simple_action("ZoomIn")->set_enabled(image && manual_zoom);
    lookup_simple_action("ZoomOut")->set_enabled(image && manual_zoom);
    lookup_simple_action("ResetZoom")->set_enabled(image && manual_zoom);
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
            lookup_action("ToggleBooruBrowser")->change_state(true);
            set_active_imagelist(page->get_imagelist());
            update_widgets_visibility();
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
        open_file(dialog->get_uri());
}

void MainWindow::on_open_recent(const Glib::VariantBase& value)
{
    Glib::Variant<Glib::ustring> s{ Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::ustring>>(
        value) };
    open_file(s.get());
}

void MainWindow::on_close()
{
    if (m_LocalImageList == m_ActiveImageList && !m_ActiveImageList->empty())
    {
        m_LocalImageList->clear();
        return;
    }
    else if (!m_BooruBrowser->get_pages().empty())
    {
        m_BooruBrowser->on_close_tab();
        return;
    }

    // We are closing the window save some settings
    quit();
}

void MainWindow::quit()
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
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 6> widget_vis{ {
        { "ToggleHideAll", "HideAll" },
        { "ToggleMenuBar", "MenuBarVisible" },
        { "ToggleStatusBar", "StatusBarVisible" },
        { "ToggleScrollbars", "ScrollbarsVisible" },
        { "ToggleBooruBrowser", "BooruBrowserVisible" },
        { "ToggleThumbnailBar", "ThumbnailBarVisible" },
    } };

    for (auto& w : widget_vis)
    {
        bool v{ false };
        lookup_action(w.first.data())->get_state(v);
        Settings.set(w.second.data(), v);
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

void MainWindow::on_toggle_fullscreen(bool state)
{
    auto hide_all_action{ lookup_action("ToggleHideAll") };

    if (is_fullscreen())
    {
        if (Settings.get_bool("HideAllFullscreen") && m_HideAllFullscreen && !m_WasHideAll)
        {
            m_HideAllFullscreen = false;
            hide_all_action->change_state(false);
        }

        unfullscreen();
    }
    else
    {
        if (Settings.get_bool("HideAllFullscreen"))
        {
            m_HideAllFullscreen = true;
            hide_all_action->get_state(m_WasHideAll);
            hide_all_action->change_state(true);
        }

        // Save this here in case the program is closed when fullscreen
        save_window_geometry();
        fullscreen();
    }

    update_widgets_visibility();
}

void MainWindow::on_toggle_manga_mode(bool state)
{
    Settings.set("MangaMode", state);

    m_ImageBox->queue_draw_image(true);
}

void MainWindow::on_toggle_menu_bar(bool state)
{
    Settings.set("MenuBarVisible", state);

    update_widgets_visibility();
}

void MainWindow::on_toggle_status_bar(bool state)
{
    Settings.set("StatusBarVisible", state);

    update_widgets_visibility();
}

void MainWindow::on_toggle_scrollbars(bool state)
{
    Settings.set("ScrollbarsVisible", state);

    update_widgets_visibility();
}

void MainWindow::on_toggle_booru_browser(bool state)
{
    Settings.set("BooruBrowserVisible", state);

    if (state)
    {
        bool hide_all{ false };
        lookup_action("ToggleHideAll")->get_state(hide_all);

        if (!hide_all)
            m_BooruBrowser->get_tag_entry()->grab_focus();

        if (m_BooruBrowser->get_active_page() &&
            m_BooruBrowser->get_active_page()->get_imagelist() != m_ActiveImageList)
            set_active_imagelist(m_BooruBrowser->get_active_page()->get_imagelist());

        auto tb_action{ lookup_action("ToggleThumbnailBar") };
        bool tb_state{ false };
        tb_action->get_state(tb_state);
        if (tb_state)
            tb_action->change_state(!tb_state);
    }

    update_widgets_visibility();
}

void MainWindow::on_toggle_thumbnail_bar(bool state)
{
    Settings.set("ThumbnailBarVisible", state);

    if (state)
    {
        if (m_ActiveImageList != m_LocalImageList)
            set_active_imagelist(m_LocalImageList);

        auto bb_action{ lookup_action("ToggleBooruBrowser") };
        bool bb_state{ false };
        bb_action->get_state(bb_state);
        if (bb_state)
            bb_action->change_state(!bb_state);
    }

    update_widgets_visibility();
}

void MainWindow::on_toggle_hide_all(bool state)
{
    // User toggled hide all after program set it to true
    if (m_HideAllFullscreen && !state)
        m_HideAllFullscreen = false;

    if (!m_HideAllFullscreen)
        Settings.set("HideAll", state);

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

void MainWindow::on_toggle_slideshow(bool)
{
    m_ImageBox->toggle_slideshow();
    if (m_ImageBox->is_slideshow_running())
        m_StatusBar->set_message(_("Slideshow started"));
    else
        m_StatusBar->set_message(_("Slideshow stopped"));
}

void MainWindow::on_toggle_auto_hide_info_box(bool state)
{
    Settings.set("AutoHideInfoBox", state);
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
        auto prefs{ Application::get_default()->get_preferences_dialog() };
        prefs->set_ask_delete_confirm(Settings.get_bool("AskDeleteConfirm"));
    }

    m_ActiveImageList->get_current()->trash();
}
