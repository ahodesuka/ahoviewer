#include <iostream>
#include <glibmm/i18n.h>

#include "browser.h"
using namespace AhoViewer::Booru;

#include "application.h"
#include "image.h"
#include "mainwindow.h"
#include "tempdir.h"

Browser::Browser(BaseObjectType *cobj, const Glib::RefPtr<Gtk::Builder> &bldr)
  : Gtk::VPaned(cobj),
    m_MinWidth(0),
    m_ClosePage(false),
    m_IconDownloaded(false),
    m_LastSavePath(Settings.get_string("LastSavePath"))
{
    bldr->get_widget("Booru::Browser::Notebook",          m_Notebook);
    bldr->get_widget("Booru::Browser::NewTabButton",      m_NewTabButton);
    bldr->get_widget("Booru::Browser::SaveImagesButton",  m_SaveImagesButton);
    bldr->get_widget("Booru::Browser::ComboBox",          m_ComboBox);
    bldr->get_widget_derived("Booru::Browser::TagEntry",  m_TagEntry);
    bldr->get_widget_derived("Booru::Browser::TagView",   m_TagView);
    bldr->get_widget_derived("StatusBar",                 m_StatusBar);
    bldr->get_widget("MainWindow::HPaned",                m_HPaned);

    m_TagEntry->signal_key_press_event().connect(
            sigc::mem_fun(*this, &Browser::on_entry_key_press_event), false);
    m_TagEntry->signal_changed().connect(sigc::mem_fun(*this, &Browser::on_entry_value_changed));

    m_TagView->signal_new_tab_tag().connect(sigc::mem_fun(*this, &Browser::on_new_tab_tag));

    m_ComboModel = Gtk::ListStore::create(m_ComboColumns);
    m_ComboBox->set_model(m_ComboModel);
    update_combobox_model();

    m_ComboBox->pack_start(m_ComboColumns.icon, false);
    m_ComboBox->pack_start(m_ComboColumns.name);
    (*m_ComboBox->get_cells().begin())->set_fixed_size(20, 16);

    m_UIManager = Glib::RefPtr<Gtk::UIManager>::cast_static(bldr->get_object("UIManager"));

    m_Notebook->set_group_name(TempDir::get_instance().get_dir());
    m_Notebook->signal_switch_page().connect(sigc::mem_fun(*this, &Browser::on_switch_page));
    m_Notebook->signal_page_removed().connect(sigc::mem_fun(*this, &Browser::on_page_removed));
    m_Notebook->signal_page_added().connect(sigc::mem_fun(*this, &Browser::on_page_added));

    g_signal_connect(m_Notebook->gobj(), "create-window", G_CALLBACK(on_create_window), this);

    set_focus_chain(std::vector<Gtk::Widget*>{ m_TagEntry });

    m_PosChangedConn = property_position().signal_changed().connect([&]()
    {
        if (get_realized())
            Settings.set("TagViewPosition", get_position());
    });
}

Browser::~Browser()
{
    delete m_Notebook;
}

std::vector<Page*> Browser::get_pages() const
{
    std::vector<Page*> pages;

    for (size_t i = 0, n = m_Notebook->get_n_pages(); i < n; ++i)
        pages.push_back(static_cast<Page*>(m_Notebook->get_nth_page(i)));

    return pages;
}

void Browser::update_combobox_model()
{
    for (sigc::connection conn : m_SiteIconConns)
        conn.disconnect();

    m_ComboChangedConn.disconnect();
    m_ComboModel->clear();

    for (std::shared_ptr<Site> site : Settings.get_sites())
    {
        Gtk::TreeIter it = m_ComboModel->append();
        sigc::connection c = site->signal_icon_downloaded().connect([ &, site, it ]
        {
            it->set_value(m_ComboColumns.icon, site->get_icon_pixbuf());
            // XXX: This is a terrible hack to fix an issue where the tagentry
            // gets wrapped and becomes invisible after a site icon is downloaded
            // and set.  Normally this issue goes away after restarting the
            // program, but for someone who opens ahoviewer for the first time
            // and has this problem occur is quite undesirable.
            if (!m_IconDownloaded)
            {
                m_IconDownloaded = true;
                m_MinWidth += 2;
                m_HPaned->set_position(m_MinWidth);
            }
        });
        m_SiteIconConns.push_back(std::move(c));

        it->set_value(m_ComboColumns.icon, site->get_icon_pixbuf());
        it->set_value(m_ComboColumns.name, site->get_name());

    }

    m_ComboChangedConn = m_ComboBox->signal_changed().connect([&]()
    {
        m_TagEntry->set_tags(get_active_site()->get_tags());
    });

    int selected = std::min(static_cast<size_t>(Settings.get_int("SelectedBooru")),
                            Settings.get_sites().size() - 1);
    m_ComboBox->set_active(selected);
}

void Browser::on_new_tab()
{
    Page *page = Gtk::manage(new Page);

    // If you type into the entry when no tab exists and hit
    // enter it will create a new tab for you
    // The tags need to be manually set in this case
    if (m_Notebook->get_n_pages() == 0)
        page->m_Tags = m_TagEntry->get_text();

    int page_num = m_Notebook->append_page(*page, *page->get_tab());

    m_Notebook->set_current_page(page_num);
    m_Notebook->set_tab_reorderable(*page, true);
    m_Notebook->set_tab_detachable(*page, true);
    gtk_container_child_set(GTK_CONTAINER(m_Notebook->gobj()),
                            GTK_WIDGET(page->gobj()), "tab-expand", TRUE, NULL);
    m_TagEntry->grab_focus();
}

void Browser::on_close_tab()
{
    if (m_Notebook->get_n_pages() > 0)
        close_page(get_active_page());
}

void Browser::on_save_image()
{
    if (get_active_page()->is_saving())
        return;

    Gtk::Window *window = static_cast<Gtk::Window*>(get_toplevel());
    Gtk::FileChooserDialog dialog(*window, "Save Image As", Gtk::FILE_CHOOSER_ACTION_SAVE);
    dialog.set_modal();

    const std::shared_ptr<Image> image =
        std::static_pointer_cast<Image>(get_active_page()->get_imagelist()->get_current());

    dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
    dialog.add_button("Save", Gtk::RESPONSE_OK);

    if (!m_LastSavePath.empty())
        dialog.set_current_folder(m_LastSavePath);

    dialog.set_current_name(Glib::path_get_basename(image->get_filename()));

    if (dialog.run() == Gtk::RESPONSE_OK)
    {
        std::string path = dialog.get_filename();
        m_LastSavePath = Glib::path_get_dirname(path);
        get_active_page()->save_image(path, image);
    }
}

void Browser::on_save_images()
{
    if (get_active_page()->is_saving())
        return;

    Gtk::Window *window = static_cast<Gtk::Window*>(get_toplevel());
    Gtk::FileChooserDialog dialog(*window, "Save Images", Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER);
    dialog.set_modal();

    dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
    dialog.add_button("Save", Gtk::RESPONSE_OK);

    if (!m_LastSavePath.empty())
        dialog.set_current_folder(m_LastSavePath);

    if (dialog.run() == Gtk::RESPONSE_OK)
    {
        std::string path = dialog.get_filename();
        m_LastSavePath = path;
        Page *page = get_active_page();

        m_SaveProgConn.disconnect();
        m_SaveProgConn = page->signal_save_progress().connect(sigc::mem_fun(this, &Browser::on_save_progress));

        page->save_images(path);
    }
}

void Browser::on_view_post()
{
    const std::shared_ptr<Image> image =
        std::static_pointer_cast<Image>(get_active_page()->get_imagelist()->get_current());

    Gio::AppInfo::launch_default_for_uri(image->get_post_url());
}

void Browser::on_copy_image_url()
{
    const std::shared_ptr<Image> image =
        std::static_pointer_cast<Image>(get_active_page()->get_imagelist()->get_current());

    Gtk::Clipboard::get()->set_text(image->get_url());
    m_StatusBar->set_message(_("Copied image URL to clipboard"));
}

void Browser::on_copy_image_data()
{
    const std::shared_ptr<Image> image =
        std::static_pointer_cast<Image>(get_active_page()->get_imagelist()->get_current());

    if (image->is_loading())
    {
        m_StatusBar->set_message(_("Cannot copy image while it is still loading"));
    }
    else if (!image->is_webm())
    {
        Gtk::Clipboard::get()->set_image(image->get_pixbuf()->get_static_image());
        // This might not actually do anything, but it's said to make the clipboard
        // persist even if this program closes (requires a clipboard manager that supports it)
        Gtk::Clipboard::get()->store();
        m_StatusBar->set_message(_("Copied image to clipboard"));
    }
}

void Browser::on_copy_post_url()
{
    const std::shared_ptr<Image> image =
        std::static_pointer_cast<Image>(get_active_page()->get_imagelist()->get_current());

    Gtk::Clipboard::get()->set_text(image->get_post_url());
    m_StatusBar->set_message(_("Copied post URL to clipboard"));
}

void Browser::on_realize()
{
    m_PopupMenu = static_cast<Gtk::Menu*>(m_UIManager->get_widget("/BooruPopupMenu"));

    // Connect buttons to their actions
    Glib::RefPtr<Gtk::ActionGroup> actionGroup =
        static_cast<std::vector<Glib::RefPtr<Gtk::ActionGroup>>>(m_UIManager->get_action_groups())[0];

    m_SaveImageAction = actionGroup->get_action("SaveImage");
    m_SaveImagesAction = actionGroup->get_action("SaveImages");

    // Gtkmm2 doesn't implement activatable completely
    GtkActivatable *able = GTK_ACTIVATABLE(m_NewTabButton->gobj());
    gtk_activatable_set_related_action(able, actionGroup->get_action("NewTab")->gobj());
    able = GTK_ACTIVATABLE(m_SaveImagesButton->gobj());
    gtk_activatable_set_related_action(able, m_SaveImagesAction->gobj());

    Gtk::VPaned::on_realize();

    get_window()->freeze_updates();
    m_PosChangedConn.block();

    while (Glib::MainContext::get_default()->pending())
        Glib::MainContext::get_default()->iteration(true);

    m_MinWidth = get_allocation().get_width();
    set_size_request(std::max(Settings.get_int("BooruWidth"), m_MinWidth), -1);
    set_position(Settings.get_int("TagViewPosition"));

    m_PosChangedConn.unblock();
    get_window()->thaw_updates();
}

void Browser::on_show()
{
    Gtk::VPaned::on_show();
    Page *page = get_active_page();

    if (page)
        page->scroll_to_selected();
}

void Browser::close_page(Page *page)
{
    if (page->ask_cancel_save())
    {
        m_ClosePage = true;
        m_Notebook->remove_page(*page);
    }
}

void Browser::on_save_progress(const Page *p)
{
    size_t c = p->m_SaveImagesCurrent,
           t = p->m_SaveImagesTotal;
    std::ostringstream ss;
    ss << "Saving "
       << p->get_site()->get_name() + (!p->m_SearchTags.empty() ? " - " + p->m_SearchTags : "")
       << " " << c << " / " << t;
    m_StatusBar->set_progress(ss.str(), static_cast<double>(c) / t, StatusBar::Priority::SAVE, c == t ? 2 : 0);

    if (c == t)
    {
        m_SaveProgConn.disconnect();
        Glib::signal_timeout().connect_seconds_once(
            sigc::hide_return(sigc::mem_fun(*this, &Browser::check_saving_page)), 2);
    }
}

void Browser::on_image_progress(const Image *bimage, double c, double t)
{
    double speed = (c / std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                                      bimage->m_Curler.get_start_time()).count());
    std::ostringstream ss;

    if (t > 0)
    {
        ss << "Downloading " << Glib::format_size(c) << " / " << Glib::format_size(t) << " @ "
           << Glib::format_size(speed) << "/s";
        m_StatusBar->set_progress(ss.str(), c / t, StatusBar::Priority::DOWNLOAD, c == t ? 2 : 0);
    }
    else
    {
         ss << "Downloading " << Glib::format_size(c) << " / ?? @ "
            << Glib::format_size(speed) << "/s";
        m_StatusBar->set_message(ss.str(), StatusBar::Priority::DOWNLOAD, c == t ? 2 : 0);
    }
}

GtkNotebook* Browser::on_create_window(GtkNotebook*, GtkWidget*,
                                       gint x, gint y, gpointer*)
{
    auto window = Application::get_instance().create_window();

    if (window)
    {
        window->move(x, y);
        return window->m_BooruBrowser->m_Notebook->gobj();
    }

    return nullptr;
}

// Finds the first page that is currently saving and connects
// it's progress signal
// returns true if a page is saving
bool Browser::check_saving_page()
{
    auto pages = get_pages();
    auto iter  = std::find_if(pages.cbegin(), pages.cend(),
                    [](const auto page) { return page->is_saving(); });

    if (iter != pages.end())
    {
        m_SaveProgConn = (*iter)->signal_save_progress().connect(
            sigc::mem_fun(*this, &Browser::on_save_progress));
        on_save_progress(*iter);
    }

    return iter != pages.end();
}

void Browser::connect_image_signals(const std::shared_ptr<Image> bimage)
{
    m_ImageErrorConn.disconnect();
    m_ImageErrorConn = bimage->signal_download_error().connect([&](const std::string &msg)
    {
        m_StatusBar->set_message(msg);
        m_StatusBar->clear_progress(StatusBar::Priority::DOWNLOAD);
        m_StatusBar->clear_message(StatusBar::Priority::DOWNLOAD);
    });

    m_ImageProgConn.disconnect();
    m_ImageProgConn = bimage->signal_progress().connect(sigc::mem_fun(*this, &Browser::on_image_progress));

    // Update progress immediatly when switching to a downloading image
    if (bimage->m_Curler.is_active())
        bimage->on_progress();
}

bool Browser::on_entry_key_press_event(GdkEventKey *e)
{
    // we only care if enter/return was pressed while shift or no modifier was down
    if ((e->keyval == GDK_Return || e->keyval == GDK_ISO_Enter || e->keyval == GDK_KP_Enter) &&
        ((e->state & GDK_SHIFT_MASK) == GDK_SHIFT_MASK || (e->state & Gtk::AccelGroup::get_default_mod_mask()) == 0))
    {
        std::string tags = m_TagEntry->get_text();
        bool new_tab = (e->state & GDK_SHIFT_MASK) == GDK_SHIFT_MASK;

        if (new_tab || m_Notebook->get_n_pages() == 0)
            on_new_tab();

        if (new_tab)
        {
            m_TagEntry->set_text(tags);
            m_TagEntry->set_position(-1);
        }

        m_TagView->clear();
        get_active_page()->search(get_active_site());
    }
    else if (e->keyval == GDK_Escape)
    {
        m_SignalEntryBlur();
    }

    return false;
}

void Browser::on_entry_value_changed()
{
    if (get_active_page())
        get_active_page()->m_Tags = m_TagEntry->get_text();
}

void Browser::on_page_removed(Gtk::Widget *w, guint)
{
    Page *page = static_cast<Page*>(w);
    auto it = m_PageCloseConns.find(page);
    if (it != m_PageCloseConns.end())
    {
        it->second.disconnect();
        m_PageCloseConns.erase(it);
    }

    if (m_Notebook->get_n_pages() == 0)
    {
        m_TagEntry->set_text("");
        m_TagView->show_favorite_tags();
        m_SaveImagesButton->set_sensitive(false);

        m_SaveProgConn.disconnect();
        m_ImageProgConn.disconnect();
        m_ImageListConn.disconnect();
        m_ImageListClearedConn.disconnect();
        m_DownloadErrorConn.disconnect();

        m_StatusBar->clear_progress(StatusBar::Priority::SAVE);
        m_StatusBar->clear_message(StatusBar::Priority::SAVE);

        m_SignalPageChanged(nullptr);

        // This page was removed by being dnd'd to another window
        // signal_idle is used in case the tab is dnd'd into the same window
        if (!m_ClosePage)
            Glib::signal_idle().connect_once([this]()
            {
                if (m_Notebook->get_n_pages() != 0)
                    return;

                MainWindow *w = static_cast<MainWindow*>(get_toplevel());
                if (w->m_LocalImageList->empty() && !w->m_OriginalWindow)
                    w->on_quit();
            });
    }

    m_ClosePage = false;
}

void Browser::on_page_added(Gtk::Widget *w, guint)
{
    Page *page = static_cast<Page*>(w);
    page->m_PopupMenu = m_PopupMenu;
    m_PageCloseConns[page] = page->signal_closed().connect(sigc::mem_fun(*this, &Browser::close_page));

    // This is needed to update a dnd'd page's vertical scrollbar position.
    // Without it the scrollbar will be in the right position but the page
    // will display as if the value was set to 0. (aka the top of the page)
    if (!page->get_imagelist()->empty())
    {
        auto v = page->get_vadjustment()->get_value();
        page->get_vadjustment()->set_value(0);
        page->get_vadjustment()->set_value(v);
    }

    // Make sure the booru browser is visible
    MainWindow *window = static_cast<MainWindow*>(get_toplevel());
    auto ha = Glib::RefPtr<Gtk::ToggleAction>::cast_static(
            window->m_ActionGroup->get_action("ToggleHideAll")),
         bb = Glib::RefPtr<Gtk::ToggleAction>::cast_static(
            window->m_ActionGroup->get_action("ToggleBooruBrowser"));

    if (ha->get_active())
        Glib::signal_idle().connect_once([ha](){ ha->set_active(false); });

    if (!bb->get_active())
        Glib::signal_idle().connect_once([bb](){ bb->set_active(); });
}

void Browser::on_switch_page(void*, guint)
{
    Page *page = get_active_page();
    std::shared_ptr<Image> bimage = nullptr;

    if (!page->get_imagelist()->empty())
        bimage = std::static_pointer_cast<Image>(page->get_imagelist()->get_current());

    m_DownloadErrorConn.disconnect();
    m_DownloadErrorConn = page->signal_no_results().connect([&](const std::string msg)
    {
        m_StatusBar->set_message(msg);
    });

    m_ImageListConn.disconnect();
    m_ImageListConn = page->get_imagelist()->signal_changed().connect(
            sigc::mem_fun(*this, &Browser::on_imagelist_changed));

    m_ImageListClearedConn.disconnect();
    m_ImageListClearedConn = page->get_imagelist()->signal_cleared().connect([&]()
    {
        if (!check_saving_page())
        {
            m_StatusBar->clear_progress(StatusBar::Priority::SAVE);
            m_StatusBar->clear_message(StatusBar::Priority::SAVE);
        }
    });

    if (!check_saving_page())
    {
        m_SaveProgConn.disconnect();
        if (bimage && bimage->is_loading())
        {
            connect_image_signals(bimage);
        }
        else
        {
            m_StatusBar->clear_progress(StatusBar::Priority::SAVE);
            m_StatusBar->clear_message(StatusBar::Priority::SAVE);
        }
    }

    m_TagEntry->set_text(page->m_Tags);

    if (page->get_site())
    {
        int index = 0;
        std::vector<std::shared_ptr<Site>> &sites = Settings.get_sites();
        std::vector<std::shared_ptr<Site>>::iterator it =
            std::find(sites.begin(), sites.end(), page->get_site());

        if (it != sites.end())
            index = it - sites.begin();

        m_ComboBox->set_active(index);

        if (bimage)
            m_TagView->set_tags(bimage->get_tags());
    }
    else
    {
        m_TagView->show_favorite_tags();
    }

    m_SignalPageChanged(page);
}

void Browser::on_imagelist_changed(const std::shared_ptr<AhoViewer::Image> &image)
{
    std::shared_ptr<Image> bimage = std::static_pointer_cast<Image>(image);
    m_TagView->set_tags(bimage->get_tags());

    m_StatusBar->clear_progress(StatusBar::Priority::DOWNLOAD);
    m_StatusBar->clear_message(StatusBar::Priority::DOWNLOAD);

    connect_image_signals(bimage);
}

void Browser::on_new_tab_tag(const std::string &tag)
{
    if (!get_active_page() || !get_active_page()->get_imagelist()->empty())
        on_new_tab();

    m_TagEntry->set_text(tag);
    m_TagView->clear();

    get_active_page()->search(get_active_site());
}
