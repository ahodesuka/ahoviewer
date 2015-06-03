#include <iostream>
#include <glibmm/i18n.h>

#include "browser.h"
using namespace AhoViewer::Booru;

#include "image.h"

static std::string readable_file_size(double s)
{
    gchar *fsize = g_format_size(s);
    std::string str(fsize);

    if (fsize)
        g_free(fsize);

    return str;
}

Browser::Browser(BaseObjectType *cobj, const Glib::RefPtr<Gtk::Builder> &bldr)
  : Gtk::VPaned(cobj),
    m_IgnorePageSwitch(false),
    m_MinWidth(0),
    m_LastSavePath(Settings.get_string("LastSavePath"))
{
    bldr->get_widget("Booru::Browser::Notebook",          m_Notebook);
    bldr->get_widget("Booru::Browser::NewTabButton",      m_NewTabButton);
    bldr->get_widget("Booru::Browser::SaveImagesButton",  m_SaveImagesButton);
    bldr->get_widget("Booru::Browser::ComboBox",          m_ComboBox);
    bldr->get_widget_derived("Booru::Browser::TagEntry",  m_TagEntry);
    bldr->get_widget_derived("Booru::Browser::TagView",   m_TagView);

    m_TagEntry->signal_activate().connect(sigc::mem_fun(*this, &Browser::on_entry_activate));
    m_TagView->set_tag_entry(m_TagEntry);

    m_ComboBox->signal_changed().connect([ this ]()
    {
        m_TagEntry->set_tags(get_active_site()->get_tags());
    });

    m_ComboModel = Gtk::ListStore::create(m_ComboColumns);
    m_ComboBox->set_model(m_ComboModel);

    // Fill the comboxbox model
    for (std::shared_ptr<Site> site : Settings.get_sites())
        site->set_row_values(*(m_ComboModel->append()));

    int selected = std::min(Settings.get_int("SelectedBooru"),
                           (int)Settings.get_sites().size() - 1);
    m_ComboBox->set_active(selected);

    m_ComboBox->pack_start(m_ComboColumns.pixbuf_column, false);
    m_ComboBox->pack_start(m_ComboColumns.text_column);
    static_cast<std::vector<Gtk::CellRenderer*>>(m_ComboBox->get_cells())[0]->set_fixed_size(20, 16);

    m_UIManager = Glib::RefPtr<Gtk::UIManager>::cast_static(bldr->get_object("UIManager"));

    m_Notebook->signal_switch_page().connect(sigc::mem_fun(*this, &Browser::on_switch_page));
    m_Notebook->signal_page_removed().connect(sigc::mem_fun(*this, &Browser::on_page_removed));
}

Browser::~Browser()
{

}

void Browser::on_new_tab()
{
    Page *page = Gtk::manage(new Page());
    page->signal_closed().connect([ this, page ]() { close_page(page); });

    int page_num = m_Notebook->append_page(*page, *page->get_tab());

    m_Notebook->set_current_page(page_num);
    m_Notebook->set_tab_reorderable(*page, true);
    gtk_container_child_set(GTK_CONTAINER(m_Notebook->gobj()),
                            (GtkWidget*)page->gobj(), "tab-expand", TRUE, NULL);
    m_TagEntry->grab_focus();
}

void Browser::on_close_tab()
{
    if (m_Notebook->get_n_pages() > 0)
        close_page(get_active_page());
}

void Browser::on_save_image()
{
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
        image->save(path);
    }
}

void Browser::on_save_images()
{
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

        m_SaveProgConn = page->signal_save_progress().connect([ this, page ](size_t c, size_t t)
        {
            std::ostringstream ss;
            ss << "Saving "
               << page->get_site()->get_name() + (!page->get_tags().empty() ? " - " + page->get_tags() : "")
               << " " << c << " / " << t;
            m_StatusBar->set_progress(c / (double)t, StatusBar::Priority::HIGH, c == t ? 2 : 0);
            m_StatusBar->set_message(ss.str(), StatusBar::Priority::HIGH, c == t ? 2 : 0);

            if (c == t)
                m_SaveProgConn.disconnect();
        });

        page->save_images(path);
    }
}

void Browser::on_realize()
{
    Gtk::VPaned::on_realize();

    // Connect buttons to their actions
    Glib::RefPtr<Gtk::ActionGroup> actionGroup =
        static_cast<std::vector<Glib::RefPtr<Gtk::ActionGroup>>>(m_UIManager->get_action_groups())[0];

    m_SaveImageAction = actionGroup->get_action("SaveImage");
    m_SaveImagesAction = actionGroup->get_action("SaveImages");

    // Gtkmm2 doesn't implement activatable completely
    GtkActivatable *able = (GtkActivatable*)m_NewTabButton->gobj();
    gtk_activatable_set_related_action(able, actionGroup->get_action("NewTab")->gobj());
    able = (GtkActivatable*)m_SaveImagesButton->gobj();
    gtk_activatable_set_related_action(able, m_SaveImagesAction->gobj());

    while (Gtk::Main::events_pending())
        Gtk::Main::iteration();

    // +1 is a cheap hack for clean startups
    // without it the tag entry will not be visible
    // until the user resizes this widget
    m_MinWidth = get_allocation().get_width() + 1;
    set_size_request(std::max(Settings.get_int("BooruWidth"), m_MinWidth), -1);
    set_position(Settings.get_int("TagViewPosition"));
}

void Browser::close_page(Page *page)
{
    if (page->is_saving())
    {
        Gtk::Window *window = static_cast<Gtk::Window*>(get_toplevel());
        Gtk::MessageDialog dialog(*window, _("Are you sure that you want to stop saving images?"),
                                  false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO, true);
        dialog.set_secondary_text(_("Closing this tab will stop the save operation."));

        int response = dialog.run();

        if (response == Gtk::RESPONSE_NO || response == Gtk::RESPONSE_DELETE_EVENT)
            return;
    }

    m_Notebook->remove_page(*page);
}

void Browser::on_entry_activate()
{
    if (m_Notebook->get_n_pages() == 0)
    {
        m_IgnorePageSwitch = true;
        on_new_tab();
    }

    m_TagView->clear();
    get_active_page()->search(get_active_site(), m_TagEntry->get_text());
    m_SignalPageChanged(get_active_page());
}

void Browser::on_page_removed(Gtk::Widget*, guint)
{
    if (m_Notebook->get_n_pages() == 0)
    {
        m_TagEntry->set_text("");
        m_TagView->clear();
        m_SaveImagesButton->set_sensitive(false);

        if (m_ImageProgConn)
        {
            m_ImageProgConn.disconnect();
            m_StatusBar->clear_message();
            m_StatusBar->clear_progress();
        }

        m_SignalPageChanged(nullptr);
    }
}

void Browser::on_switch_page(void*, guint)
{
    Page *page = get_active_page();

    m_NoResultsConn.disconnect();
    m_NoResultsConn = page->signal_no_results().connect([ this, page ]()
    {
        std::ostringstream ss;
        ss << "No results found"
           << (!page->get_tags().empty() ? " for '" + page->get_tags() + "'" : "")
           << " on " << page->get_site()->get_name();
        m_StatusBar->set_message(ss.str());
    });

    m_ImageListConn.disconnect();
    m_ImageListConn = page->get_imagelist()->signal_changed().connect(
            sigc::mem_fun(*this, &Browser::on_imagelist_changed));

    if (m_IgnorePageSwitch)
    {
        m_IgnorePageSwitch = false;
        return;
    }

    m_TagEntry->set_text(page->get_tags());

    if (page->get_site())
    {
        int index = 0;
        std::vector<std::shared_ptr<Site>> sites = Settings.get_sites();
        std::vector<std::shared_ptr<Site>>::iterator it =
            std::find(sites.begin(), sites.end(), page->get_site());

        if (it != sites.end())
            index = it - sites.begin();

        m_ComboBox->set_active(index);
    }

    m_SignalPageChanged(page);
}

void Browser::on_imagelist_changed(const std::shared_ptr<AhoViewer::Image> &image)
{
    std::shared_ptr<Image> bimage = std::static_pointer_cast<Image>(image);
    m_TagView->set_tags(bimage->get_tags());

    if (m_ImageProgConn)
    {
        m_ImageProgConn.disconnect();
        m_StatusBar->clear_message();
        m_StatusBar->clear_progress();
    }

    m_ImageProgConn = bimage->signal_progress().connect([ this, bimage ](double c, double t)
    {
        double speed = (c / std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                                          bimage->get_curler()->get_start_time()).count());
        std::ostringstream ss;

        if (t > 0)
        {
            ss << "Downloading " << readable_file_size(c) << " / " << readable_file_size(t) << " @ "
               << readable_file_size(speed) << "/s";
            m_StatusBar->set_progress(c / t, StatusBar::Priority::NORMAL, 2);
        }
        else
        {
             ss << "Downloading " << readable_file_size(c) << " / ?? @ "
                << readable_file_size(speed) << "/s";
        }
        m_StatusBar->set_message(ss.str(), StatusBar::Priority::NORMAL, 2);
    });
}
