#include <glibmm/i18n.h>

#include "siteeditor.h"
using namespace AhoViewer;

#include "settings.h"

// Fixes issue with winnt.h
#ifdef DELETE
#undef DELETE
#endif

SiteEditor::SiteEditor(BaseObjectType *cobj, const Glib::RefPtr<Gtk::Builder> &bldr)
  : Gtk::TreeView(cobj),
    m_Model(Gtk::ListStore::create(m_Columns)),
    m_NameColumn(Gtk::manage(new Gtk::TreeView::Column(_("Name"), m_Columns.name))),
    m_UrlColumn(Gtk::manage(new Gtk::TreeView::Column(_("Url"), m_Columns.url))),
    m_Sites(Settings.get_sites()),
    m_ErrorPixbuf(Gtk::Invisible().render_icon(Gtk::Stock::DELETE, Gtk::ICON_SIZE_MENU)),
    m_SiteCheckEdit(false),
    m_SiteCheckEditSuccess(false),
    m_SiteCheckThread(nullptr)
{
    m_SignalSiteChecked.connect(sigc::mem_fun(*this, &SiteEditor::on_site_checked));

    for (const std::shared_ptr<Booru::Site> &s : m_Sites)
    {
        Gtk::TreeIter iter = m_Model->append();
        iter->set_value(m_Columns.icon, s->get_icon_pixbuf());
        s->signal_icon_downloaded().connect([ this, iter, s ]()
                { iter->set_value(m_Columns.icon, s->get_icon_pixbuf()); });
        iter->set_value(m_Columns.name, s->get_name());
        iter->set_value(m_Columns.url,  s->get_url());
        iter->set_value(m_Columns.site, s);
    }

    CellRendererIcon *iconRenderer = Gtk::manage(new CellRendererIcon(this));
    iconRenderer->property_xpad() = 2;
    iconRenderer->property_ypad() = 2;
    append_column("", *iconRenderer);
    get_column(0)->add_attribute(iconRenderer->property_loading(), m_Columns.loading);
    get_column(0)->add_attribute(iconRenderer->property_pulse(), m_Columns.pulse);
    get_column(0)->add_attribute(iconRenderer->property_pixbuf(), m_Columns.icon);

    Gtk::CellRendererText *textRenderer = nullptr;

    textRenderer = static_cast<Gtk::CellRendererText*>(m_NameColumn->get_first_cell());
    textRenderer->property_editable() = true;
    textRenderer->signal_edited().connect(sigc::mem_fun(*this, &SiteEditor::on_name_edited));
    append_column(*m_NameColumn);

    textRenderer = static_cast<Gtk::CellRendererText*>(m_UrlColumn->get_first_cell());
    textRenderer->property_editable() = true;
    textRenderer->signal_edited().connect(sigc::mem_fun(*this, &SiteEditor::on_url_edited));
    append_column(*m_UrlColumn);

    set_model(m_Model);
    get_selection()->select(m_Model->get_iter("0"));

    Gtk::ToolButton *toolButton = nullptr;
    bldr->get_widget("DeleteSiteButton", toolButton);
    toolButton->signal_clicked().connect(sigc::mem_fun(*this, &SiteEditor::delete_site));

    bldr->get_widget("AddSiteButton", toolButton);
    toolButton->signal_clicked().connect(sigc::mem_fun(*this, &SiteEditor::add_row));
}

SiteEditor::~SiteEditor()
{
    if (m_SiteCheckThread)
    {
        m_SiteCheckThread->join();
        m_SiteCheckThread = nullptr;
    }
}

void SiteEditor::add_row()
{
    Gtk::TreePath path(m_Model->append());
    set_cursor(path, *m_NameColumn, true);
    scroll_to_row(path);
}

void SiteEditor::delete_site()
{
    Gtk::TreeIter o = get_selection()->get_selected();

    m_Sites.erase(std::remove(m_Sites.begin(), m_Sites.end(), o->get_value(m_Columns.site)), m_Sites.end());
    Settings.update_sites();
    m_SignalEdited();

    Gtk::TreeIter n = m_Model->erase(o);
    get_selection()->select(n ? n : --n);
}

void SiteEditor::on_name_edited(const std::string &p, const std::string &text)
{
    Gtk::TreePath path(p);
    Gtk::TreeIter iter = m_Model->get_iter(path);
    std::string name(text);

    // make sure the site name is unique
    for (size_t i = 1; !is_name_unique(iter, name); ++i)
        name = text + std::to_string(i);

    iter->set_value(m_Columns.name, name);

    // start editing url column if it's blank
    if (iter->get_value(m_Columns.url).empty())
        set_cursor(path, *m_UrlColumn, true);
    else
        add_edit_site(iter);
}

void SiteEditor::on_url_edited(const std::string &p, const std::string &text)
{
    Gtk::TreePath path(p);
    Gtk::TreeIter iter = m_Model->get_iter(path);
    std::string url(text);

    if (url.back() == '/')
        url = text.substr(0, text.size() - 1);

    iter->set_value(m_Columns.url, url);
    add_edit_site(iter);
}

bool SiteEditor::is_name_unique(const Gtk::TreeIter &iter, const std::string &name) const
{
    for (const Gtk::TreeIter &i : m_Model->children())
        if (i->get_value(m_Columns.name) == name && iter != i)
            return false;

    return true;
}

void SiteEditor::add_edit_site(const Gtk::TreeIter &iter)
{
    std::string name(iter->get_value(m_Columns.name)),
                url(iter->get_value(m_Columns.url));

    if (name.empty() || url.empty())
    {
        iter->set_value(m_Columns.icon, m_ErrorPixbuf);
        return;
    }

    if (m_SiteCheckThread)
        m_SiteCheckThread->join();

    m_SiteCheckIter = iter;

    std::vector<std::shared_ptr<Booru::Site>>::iterator i =
        std::find(m_Sites.begin(), m_Sites.end(), iter->get_value(m_Columns.site));
    std::shared_ptr<Booru::Site> site = i != m_Sites.end() ? *i : nullptr;

    // editting
    if (site)
    {
        if (name == site->get_name() && url == site->get_url())
        {
            if (iter->get_value(m_Columns.icon) == m_ErrorPixbuf)
                iter->set_value(m_Columns.icon, site->get_icon_pixbuf());

            return;
        }

        m_SiteCheckEdit = true;
        m_SiteCheckSite = site;
        m_SiteCheckIter->set_value(m_Columns.loading, true);
        m_SiteCheckThread = Glib::Threads::Thread::create([ this, name, url ]()
        {
            m_SiteCheckSite->set_name(name);
            m_SiteCheckEditSuccess = m_SiteCheckSite->set_url(url);

            if (m_SiteCheckEditSuccess)
                update_edited_site_icon();

            m_SignalSiteChecked();
        });
    }
    else
    {
        m_SiteCheckEdit = false;
        m_SiteCheckIter->set_value(m_Columns.loading, true);
        m_SiteCheckThread = Glib::Threads::Thread::create([ this, name, url ]()
        {
            m_SiteCheckSite = Booru::Site::create(name, url);

            if (m_SiteCheckSite)
                update_edited_site_icon();

            m_SignalSiteChecked();
        });
    }
}

void SiteEditor::update_edited_site_icon()
{
    m_SiteCheckIter->set_value(m_Columns.icon, m_SiteCheckSite->get_icon_pixbuf(true));
    m_SiteCheckIter->set_value(m_Columns.loading, false);
}

void SiteEditor::on_site_checked()
{
    if ((m_SiteCheckEdit && !m_SiteCheckEditSuccess) || (!m_SiteCheckEdit && !m_SiteCheckSite))
    {
        m_SiteCheckIter->set_value(m_Columns.icon, m_ErrorPixbuf);
        m_SiteCheckIter->set_value(m_Columns.loading, false);
    }
    else if (m_SiteCheckSite && !m_SiteCheckEdit)
    {
        m_Sites.push_back(m_SiteCheckSite);
        m_SiteCheckIter->set_value(m_Columns.site, m_SiteCheckSite);
    }

    if (m_SiteCheckEdit || m_SiteCheckSite)
    {
        Settings.update_sites();
        m_SignalEdited();
    }

    m_SiteCheckThread->join();
    m_SiteCheckThread = nullptr;
}
