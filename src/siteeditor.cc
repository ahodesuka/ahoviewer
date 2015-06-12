#include <glibmm/i18n.h>
#include <iostream>

#include "siteeditor.h"
using namespace AhoViewer;

#include "settings.h"

SiteEditor::SiteEditor(BaseObjectType *cobj, const Glib::RefPtr<Gtk::Builder> &bldr)
  : Gtk::TreeView(cobj),
    m_Model(Gtk::ListStore::create(m_Columns)),
    m_NameColumn(Gtk::manage(new Gtk::TreeView::Column(_("Name"), m_Columns.name))),
    m_UrlColumn(Gtk::manage(new Gtk::TreeView::Column(_("Url"), m_Columns.url))),
    m_Sites(Settings.get_sites()),
    m_ErrorPixbuf(Gtk::Invisible().render_icon(Gtk::Stock::DELETE, Gtk::ICON_SIZE_MENU))
{
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

    Gtk::CellRendererPixbuf *iconRenderer = Gtk::manage(new Gtk::CellRendererPixbuf());
    iconRenderer->property_xpad() = 2;
    iconRenderer->property_ypad() = 2;
    append_column("", *iconRenderer);
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

    iter->set_value(m_Columns.url, text);
    add_edit_site(iter);
}

bool SiteEditor::is_name_unique(const Gtk::TreeIter &iter, const std::string &name) const
{
    Gtk::TreeModel::Children children = m_Model->children();
    for (Gtk::TreeIter i = children.begin(); i != children.end(); ++i)
    {
        if (name == i->get_value(m_Columns.name) && iter != i)
        {
            return false;
        }
    }

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

    std::vector<std::shared_ptr<Booru::Site>>::iterator i =
        std::find(m_Sites.begin(), m_Sites.end(), iter->get_value(m_Columns.site));
    std::shared_ptr<Booru::Site> site = i != m_Sites.end() ? *i : nullptr;

    // editting
    if (site)
    {
        if (name != site->get_name() || url != site->get_url())
        {
            site->set_name(name);

            if (!site->set_url(url))
                iter->set_value(m_Columns.icon, m_ErrorPixbuf);
            else
                iter->set_value(m_Columns.icon, site->get_icon_pixbuf());
        }
        else
        {
            return;
        }
    }
    else
    {
        // FIXME: This locks up the ui for a second when
        // get_type_from_url is called, mabye do it async
        // as does the above Site::set_url call
        site = Booru::Site::create(name, url);

        if (site)
        {
            m_Sites.push_back(site);
            site->signal_icon_downloaded().connect([ this, iter, site ]()
                    { iter->set_value(m_Columns.icon, site->get_icon_pixbuf()); });
            iter->set_value(m_Columns.icon, site->get_icon_pixbuf());
            iter->set_value(m_Columns.site, site);
        }
        else
        {
            iter->set_value(m_Columns.icon, m_ErrorPixbuf);
            return;
        }
    }

    Settings.update_sites();
    m_SignalEdited();
}
