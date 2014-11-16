#include <iostream>

#include "browser.h"
using namespace AhoViewer::Booru;

#include "image.h"

Browser::Browser(BaseObjectType *cobj, const Glib::RefPtr<Gtk::Builder> &bldr)
  : Gtk::VPaned(cobj),
    m_MinWidth(0),
    m_ImageFetcher(new ImageFetcher())
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

    m_ComboBox->set_active(Settings.get_int("SelectedBooru"));

    m_ComboBox->pack_start(m_ComboColumns.pixbuf_column, false);
    m_ComboBox->pack_start(m_ComboColumns.text_column);
    static_cast<std::vector<Gtk::CellRenderer*>>(m_ComboBox->get_cells())[0]->set_fixed_size(18, 16);

    m_UIManager = Glib::RefPtr<Gtk::UIManager>::cast_static(bldr->get_object("UIManager"));

    m_Notebook->signal_switch_page().connect(sigc::mem_fun(*this, &Browser::on_switch_page));
    m_Notebook->signal_page_removed().connect(sigc::mem_fun(*this, &Browser::on_page_removed));
}

Browser::~Browser()
{

}

void Browser::on_new_tab()
{
    Page *page = Gtk::manage(new Page(this));
    page->signal_closed().connect(sigc::mem_fun(*this, &Browser::on_page_closed));

    int page_num = m_Notebook->append_page(*page, *page->get_tab());

    m_Notebook->set_current_page(page_num);
    m_Notebook->set_tab_reorderable(*page, true);
    gtk_container_child_set(GTK_CONTAINER(m_Notebook->gobj()),
                            (GtkWidget*)page->gobj(), "tab-expand", TRUE, NULL);
    m_TagEntry->grab_focus();
}

/**
 * For the close tab action.
 **/
void Browser::on_close_tab()
{
    if (m_Notebook->get_n_pages() > 0)
        on_page_closed(get_active_page());
}

void Browser::on_save_images()
{
    std::cout << "on_save_images" << std::endl;
}

void Browser::on_realize()
{
    Gtk::VPaned::on_realize();

    // Connect buttons to their actions
    Glib::RefPtr<Gtk::ActionGroup> actionGroup =
        static_cast<std::vector<Glib::RefPtr<Gtk::ActionGroup>>>(m_UIManager->get_action_groups())[0];

    m_SaveImagesAction = actionGroup->get_action("SaveImages");
    m_SaveImagesAction->set_sensitive(false);

    // Gtkmm2 doesn't implement activatable completely
    GtkActivatable *able = (GtkActivatable*)m_NewTabButton->gobj();
    gtk_activatable_set_related_action(able, actionGroup->get_action("NewTab")->gobj());
    able = (GtkActivatable*)m_SaveImagesButton->gobj();
    gtk_activatable_set_related_action(able, m_SaveImagesAction->gobj());

    while (Gtk::Main::events_pending())
        Gtk::Main::iteration();

    m_MinWidth = get_allocation().get_width();
    set_size_request(std::max(Settings.get_int("BooruWidth"), m_MinWidth), -1);
    set_position(Settings.get_int("TagViewPosition"));
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

/**
 * For when the close button on the tab is pressed.
 **/
void Browser::on_page_closed(Page *page)
{
    m_Notebook->remove_page(*page);
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
        ss << "No results found for '" << page->get_tags()
           << "' on " << page->get_site()->get_name();
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

    m_ImageProgConn = bimage->get_curler()->signal_progress().connect([ this, bimage ]()
    {
        double c, t;
        bimage->get_curler()->get_progress(c, t);

        if (t > 0)
        {
            double speed = (c / std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                                              bimage->get_curler()->get_start_time()).count());
            std::ostringstream ss;
            ss << "Downloading " << readable_file_size(c) << " / " << readable_file_size(t) << " @ "
               << readable_file_size(speed) << "/s";
            m_StatusBar->set_progress(c / t, StatusBar::Priority::NORMAL, 2);
            m_StatusBar->set_message(ss.str(), StatusBar::Priority::NORMAL, 2);
        }
    });
}

std::string Browser::readable_file_size(double s)
{
    gchar *fsize = g_format_size(s);
    std::string str(fsize);

    if (fsize)
        g_free(fsize);

    return str;
}
