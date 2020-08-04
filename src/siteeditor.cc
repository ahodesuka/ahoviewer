#include "siteeditor.h"

#include <gdk/gdkkeysyms-compat.h>
#include <glibmm/i18n.h>
using namespace AhoViewer;
using namespace AhoViewer::Booru;

#include "booru/site.h"
#include "settings.h"
#include "util.h"

SiteEditor::SiteEditor(BaseObjectType* cobj, const Glib::RefPtr<Gtk::Builder>& bldr)
    : Gtk::TreeView(cobj),
      m_Model(Gtk::ListStore::create(m_Columns)),
      m_NameColumn(Gtk::manage(new Gtk::TreeView::Column(_("Name"), m_Columns.name))),
      m_UrlColumn(Gtk::manage(new Gtk::TreeView::Column(_("Url"), m_Columns.url))),
      m_SampleColumn(Gtk::manage(new Gtk::TreeView::Column(_("Samples"), m_Columns.samples))),
      m_Sites(Settings.get_sites())
{
    bldr->get_widget("BooruRegisterLinkButton", m_RegisterButton);
    bldr->get_widget("BooruUsernameEntry", m_UsernameEntry);
    bldr->get_widget("BooruPasswordEntry", m_PasswordEntry);
    bldr->get_widget("BooruPasswordLabel", m_PasswordLabel);

    m_SignalSiteChecked.connect(sigc::mem_fun(*this, &SiteEditor::on_site_checked));

    for (const std::shared_ptr<Site>& s : m_Sites)
    {
        Gtk::TreeIter iter{ m_Model->append() };
        iter->set_value(m_Columns.icon, s->get_icon_pixbuf());
        s->signal_icon_downloaded().connect(
            [&, iter, s]() { iter->set_value(m_Columns.icon, s->get_icon_pixbuf()); });
        iter->set_value(m_Columns.name, s->get_name());
        iter->set_value(m_Columns.url, s->get_url());
        iter->set_value(m_Columns.samples, s->use_samples());
        iter->set_value(m_Columns.site, s);
    }

    try
    {
        m_ErrorPixbuf = Gtk::IconTheme::get_default()->load_icon(
            "edit-delete",
            Gtk::ICON_SIZE_MENU,
            Gtk::ICON_LOOKUP_USE_BUILTIN | Gtk::ICON_LOOKUP_GENERIC_FALLBACK);
        // TODO: Capture and print something useful
    }
    catch (...)
    {
    }

    CellRendererIcon* icon_renderer{ Gtk::manage(new CellRendererIcon(this)) };
    icon_renderer->property_xpad() = 2;
    icon_renderer->property_ypad() = 2;
    append_column("", *icon_renderer);
    get_column(0)->add_attribute(icon_renderer->property_loading(), m_Columns.loading);
    get_column(0)->add_attribute(icon_renderer->property_pulse(), m_Columns.pulse);
    get_column(0)->add_attribute(icon_renderer->property_pixbuf(), m_Columns.icon);

    Gtk::CellRendererText* text_renderer{ nullptr };

    text_renderer = static_cast<Gtk::CellRendererText*>(m_NameColumn->get_first_cell());
    text_renderer->property_editable() = true;
    text_renderer->signal_edited().connect(sigc::mem_fun(*this, &SiteEditor::on_name_edited));
    append_column(*m_NameColumn);

    text_renderer = static_cast<Gtk::CellRendererText*>(m_UrlColumn->get_first_cell());
    text_renderer->property_editable() = true;
    text_renderer->signal_edited().connect(sigc::mem_fun(*this, &SiteEditor::on_url_edited));
    m_UrlColumn->set_expand(true);
    append_column(*m_UrlColumn);

    auto* toggle_renderer = static_cast<Gtk::CellRendererToggle*>(m_SampleColumn->get_first_cell());
    toggle_renderer->signal_toggled().connect(
        sigc::mem_fun(*this, &SiteEditor::on_samples_toggled));
    toggle_renderer->set_activatable(true);
    append_column(*m_SampleColumn);

    set_model(m_Model);
    get_selection()->select(m_Model->children().begin());

    Gtk::ToolButton* tool_button{ nullptr };
    bldr->get_widget("DeleteSiteButton", tool_button);
    tool_button->signal_clicked().connect(sigc::mem_fun(*this, &SiteEditor::delete_site));

    bldr->get_widget("AddSiteButton", tool_button);
    tool_button->signal_clicked().connect(sigc::mem_fun(*this, &SiteEditor::add_row));

    m_UsernameConn = m_UsernameEntry->signal_changed().connect(
        sigc::mem_fun(*this, &SiteEditor::on_username_edited));
    m_PasswordConn = m_PasswordEntry->signal_changed().connect(
        sigc::mem_fun(*this, &SiteEditor::on_password_edited));

#ifdef HAVE_LIBSECRET
    // Make sure the initially selected site's password gets set in the entry once it's loaded
    const std::shared_ptr<Site>& s{ get_selection()->get_selected()->get_value(m_Columns.site) };
    if (s)
        s->signal_password_lookup().connect([&, s]() {
            m_PasswordConn.block();
            m_PasswordEntry->set_text(s->get_password());
            m_PasswordConn.unblock();
        });
#endif // HAVE_LIBSECRET

    m_PasswordEntry->set_visibility(false);

    // Set initial values for entries and linkbutton
    on_cursor_changed();

    // Set a flag to let the on_row_changed signal handler know that the change
    // is from a DND'ing
    m_RowInsertedConn = m_Model->signal_row_inserted().connect(
        [&](const Gtk::TreePath&, const Gtk::TreeIter& iter) { m_LastAddFromDND = true; });
    m_Model->signal_row_changed().connect(sigc::mem_fun(*this, &SiteEditor::on_row_changed));
}

SiteEditor::~SiteEditor()
{
    if (m_SiteCheckThread.joinable())
        m_SiteCheckThread.join();
}

bool SiteEditor::on_key_release_event(GdkEventKey* e)
{
    if (e->keyval == GDK_Return || e->keyval == GDK_ISO_Enter || e->keyval == GDK_KP_Enter ||
        e->keyval == GDK_Tab)
    {
        Gtk::TreeIter iter{ get_selection()->get_selected() };
        Gtk::TreePath p(iter);

        if (iter->get_value(m_Columns.url).empty())
            set_cursor(p, *m_UrlColumn, true);
        else if (iter->get_value(m_Columns.name).empty())
            set_cursor(p, *m_NameColumn, true);
    }

    return false;
}

void SiteEditor::on_cursor_changed()
{
    Gtk::TreeView::on_cursor_changed();
    std::shared_ptr<Site> s{ nullptr };

    if (get_selection()->get_selected())
        s = get_selection()->get_selected()->get_value(m_Columns.site);

    m_UsernameConn.block();
    m_PasswordConn.block();

    if (s)
    {
        m_RegisterButton->set_label(_("Register account on ") + s->get_name());
        m_RegisterButton->set_uri(s->get_register_uri());

        m_UsernameEntry->set_text(s->get_username());
        m_PasswordEntry->set_text(s->get_password());
    }
    else
    {
        m_RegisterButton->set_label(_("Register account"));

        m_UsernameEntry->set_text("");
        m_PasswordEntry->set_text("");
    }

    m_UsernameConn.unblock();
    m_PasswordConn.unblock();

    m_RegisterButton->set_sensitive(!!s);
    m_UsernameEntry->set_sensitive(!!s);
    m_PasswordEntry->set_sensitive(!!s);

    if (!s || s->get_type() == Type::GELBOORU)
        m_PasswordLabel->set_text(_("Password:"));
    else
        m_PasswordLabel->set_text(_("API Key:"));
}

void SiteEditor::add_row()
{
    m_RowInsertedConn.block();
    Gtk::TreePath path(m_Model->append());
    m_RowInsertedConn.unblock();

    set_cursor(path, *m_NameColumn, true);
    scroll_to_row(path);
}

void SiteEditor::delete_site()
{
    Gtk::TreeIter o{ get_selection()->get_selected() };

    if (o)
    {
        m_Sites.erase(std::remove(m_Sites.begin(), m_Sites.end(), o->get_value(m_Columns.site)),
                      m_Sites.end());
        m_SignalEdited();

        Gtk::TreeIter n{ m_Model->erase(o) };
        if (m_Model->children().size())
            get_selection()->select(n ? n : --n);
        on_cursor_changed();
    }
}

void SiteEditor::on_name_edited(const std::string& p, const std::string& text)
{
    Gtk::TreePath path{ p };
    Gtk::TreeIter iter{ m_Model->get_iter(path) };
    std::string name{ text };

    // make sure the site name is unique
    for (size_t i{ 1 }; !is_name_unique(iter, name); ++i)
        name = text + std::to_string(i);

    iter->set_value(m_Columns.name, name);

    if (!iter->get_value(m_Columns.url).empty())
        add_edit_site(iter);
}

void SiteEditor::on_url_edited(const std::string& p, const std::string& text)
{
    Gtk::TreePath path{ p };
    Gtk::TreeIter iter{ m_Model->get_iter(path) };
    std::string url{ text };

    if (url.back() == '/')
        url = text.substr(0, text.size() - 1);

    iter->set_value(m_Columns.url, url);
    add_edit_site(iter);
}

void SiteEditor::on_samples_toggled(const std::string& p)
{
    Gtk::TreePath path{ p };
    Gtk::TreeIter iter{ m_Model->get_iter(path) };
    bool active{ iter->get_value(m_Columns.samples) };
    iter->set_value(m_Columns.samples, !active);

    std::shared_ptr<Site> s{ iter->get_value(m_Columns.site) };
    if (s)
        s->set_use_samples(iter->get_value(m_Columns.samples));
}

void SiteEditor::on_row_changed(const Gtk::TreePath& path, const Gtk::TreeIter& iter)
{
    if (!m_LastAddFromDND)
        return;

    m_LastAddFromDND = false;
    std::shared_ptr<Site> s{ iter->get_value(m_Columns.site) };
    if (s)
    {
        Gtk::TreePath other_path{ iter };
        bool after{ false };

        if (other_path.prev())
            after = true;
        else
            other_path.next();

        std::shared_ptr<Site> s2{ m_Model->get_iter(other_path)->get_value(m_Columns.site) };
        if (s != s2)
        {
            auto i{ std::find(m_Sites.begin(), m_Sites.end(), s) },
                i2{ std::find(m_Sites.begin(), m_Sites.end(), s2) };

            // Insert after s2
            if (after)
                ++i2;

            if (i < i2)
                std::rotate(i, i + 1, i2);
            else
                std::rotate(i2, i, i + 1);

            // Update the combobox
            m_SignalEdited();

            // Calling this directly doesn't work, something handling this
            // signal later one might be setting the selection?  Either way
            // queuing it works fine.
            Glib::signal_idle().connect_once([&, iter]() { get_selection()->select(iter); });
        }
    }
}

bool SiteEditor::is_name_unique(const Gtk::TreeIter& iter, const std::string& name) const
{
    for (const Gtk::TreeIter& i : m_Model->children())
        if (i->get_value(m_Columns.name) == name && iter != i)
            return false;

    return true;
}

void SiteEditor::add_edit_site(const Gtk::TreeIter& iter)
{
    std::string name{ iter->get_value(m_Columns.name) }, url{ iter->get_value(m_Columns.url) };

    if (name.empty() || url.empty())
    {
        iter->set_value(m_Columns.icon, m_ErrorPixbuf);
        return;
    }

    if (m_SiteCheckThread.joinable())
        m_SiteCheckThread.join();

    m_SiteCheckIter = iter;

    std::vector<std::shared_ptr<Site>>::iterator i{ std::find(
        m_Sites.begin(), m_Sites.end(), iter->get_value(m_Columns.site)) };
    std::shared_ptr<Site> site{ i != m_Sites.end() ? *i : nullptr };

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
        m_SiteCheckThread = std::thread([&, name, url]() {
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
        m_SiteCheckThread = std::thread([&, name, url]() {
            m_SiteCheckSite = Site::create(name, url);

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
        on_cursor_changed();
    }

    if (m_SiteCheckEdit || m_SiteCheckSite)
        m_SignalEdited();

    if (m_SiteCheckThread.joinable())
        m_SiteCheckThread.join();
}

void SiteEditor::on_username_edited()
{
    const std::shared_ptr<Site>& s{ get_selection()->get_selected()->get_value(m_Columns.site) };
    if (s)
        s->set_username(m_UsernameEntry->get_text());
}

void SiteEditor::on_password_edited()
{
    const std::shared_ptr<Site>& s{ get_selection()->get_selected()->get_value(m_Columns.site) };
    if (s)
        s->set_password(m_PasswordEntry->get_text());
}
