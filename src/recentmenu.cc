#include "recentmenu.h"
using namespace AhoViewer;

#include "archive/archive.h"
#include "config.h"

// The maxiumum number of recent files  to display
#define MAX_RECENT_ITEMS 10
// The max width a filename can be in characters before being truncated
#define MAX_WIDTH_CHARS 36

RecentMenu::RecentMenu() : Gio::Menu{}
{
    for (const auto& format : Gdk::Pixbuf::get_formats())
        for (const auto& mime_type : format.get_mime_types())
            m_MimeTypes.emplace_back(mime_type);

#ifdef HAVE_GSTREAMER
    m_MimeTypes.emplace_back("video/mp4");
    m_MimeTypes.emplace_back("video/webm");
#endif // HAVE_GSTREAMER

    for (const auto& mime_type : Archive::MimeTypes)
        m_MimeTypes.emplace_back(mime_type);

    Gtk::RecentManager::get_default()->signal_changed().connect(
        sigc::mem_fun(*this, &RecentMenu::on_changed));

    // Populate the initial menu
    on_changed();
}

Glib::RefPtr<RecentMenu> RecentMenu::create()
{
    return Glib::RefPtr<RecentMenu>(new RecentMenu());
}

void RecentMenu::on_changed()
{
    // start from nothing
    remove_all();

    size_t n_items{ 0 };
    for (const auto& f : Gtk::RecentManager::get_default()->get_items())
    {
        // Check if it's a supported mimetype
        if (std::find(m_MimeTypes.begin(), m_MimeTypes.end(), f->get_mime_type()) !=
            m_MimeTypes.end())
        {
            Glib::ustring name{ f->get_short_name() };
            if (name.length() > MAX_WIDTH_CHARS)
                name = name.substr(0, MAX_WIDTH_CHARS) + u8"\u2026";

            auto menu_item{ Gio::MenuItem::create(name, "") };
            auto uri{ Glib::Variant<Glib::ustring>::create(f->get_uri()) };
            menu_item->set_action_and_target("win.OpenRecent", uri);

            append_item(menu_item);

            if (++n_items >= MAX_RECENT_ITEMS)
                break;
        }
    }
}
