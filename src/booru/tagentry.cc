#include "tagentry.h"
using namespace AhoViewer::Booru;

TagEntry::TagEntry(BaseObjectType* cobj, const Glib::RefPtr<Gtk::Builder>& bldr) : Gtk::Entry(cobj)
{
    m_TagCompletion = Glib::RefPtr<Gtk::EntryCompletion>::cast_static(
        bldr->get_object("Booru::Browser::TagEntryCompletion"));

    m_Model = Gtk::ListStore::create(m_Columns);

    m_TagCompletion->set_match_func(sigc::mem_fun(*this, &TagEntry::match_func));
    m_TagCompletion->set_model(m_Model);
    m_TagCompletion->signal_match_selected().connect(
        sigc::mem_fun(*this, &TagEntry::on_match_selected), false);
    m_TagCompletion->signal_cursor_on_match().connect(
        sigc::mem_fun(*this, &TagEntry::on_cursor_on_match), false);

    m_ChangedConn = signal_changed().connect(sigc::mem_fun(*this, &TagEntry::on_text_changed));

    auto* c{ Gtk::make_managed<Gtk::CellRendererText>() };
    c->property_ellipsize() = Pango::ELLIPSIZE_END;
    m_TagCompletion->pack_start(*c);
    m_TagCompletion->set_cell_data_func(*c, sigc::mem_fun(*this, &TagEntry::on_tag_cell_data));
}

void TagEntry::set_tags(const std::set<Tag>& tags)
{
    m_Tags = &tags;
}

bool TagEntry::on_key_press_event(GdkEventKey* e)
{
    return Gtk::Entry::on_key_press_event(e);
}

void TagEntry::on_grab_focus()
{
    Gtk::Entry::on_grab_focus();

    select_region(0, 0);
    set_position(-1);
}

// Finds matching tags in the m_Tags set and adds them to the model
void TagEntry::on_text_changed()
{
    if (get_text().empty())
        return;

    int spos{ get_position() };

    size_t pos{ get_text().substr(0, spos).find_last_of(' ') };
    std::string key{ pos == std::string::npos ? get_text().substr(0, spos + 1)
                                              : get_text().substr(pos + 1, spos - pos) };

    if (key.empty())
        return;

    if (key.back() == ' ')
        key.pop_back();

    // Check if key is prefixed
    if (key.front() == '-')
        key.erase(key.begin());

    m_Model->clear();

    if (key.length() >= static_cast<size_t>(m_TagCompletion->get_minimum_key_length()))
    {
        size_t i{ 0 };
        auto [it, fgr]{ m_Tags->equal_range(key) };

        while (it != m_Tags->end() && it->tag.compare(0, key.length(), key) == 0)
        {
            Gtk::TreeIter iter = m_Model->append();
            iter->set_value(m_Columns.tag, *it++);

            // Limit list to 20 tags
            if (++i >= 20)
                break;
        }
    }

    m_TagCompletion->complete();
}

// Temporarly inserts selected matches into the entry (when moving between elements in the popup)
bool TagEntry::on_cursor_on_match(const Gtk::TreeIter& iter)
{
    const std::string tag{ iter->get_value(m_Columns.tag) };
    int spos, epos;
    get_selection_bounds(spos, epos);

    size_t pos{ get_text().substr(0, spos).find_last_of(' ') };
    std::string prefix{ pos == std::string::npos ? (get_text()[0] == '-' ? "-" : "")
                                                 : get_text().substr(0, pos + 1) },
        suffix{ get_text().substr(epos) };

    if (pos != std::string::npos && get_text().substr(pos + 1, 1) == "-")
        prefix += '-';

    m_ChangedConn.block();
    set_text(prefix + tag + suffix);
    m_ChangedConn.unblock();

    // This is used to keep track of what part of the tag was completed
    select_region(spos, tag.size() - (spos - prefix.size()) + spos);

    return true;
}

// Inserts the selected tag into the entry
bool TagEntry::on_match_selected(const Gtk::TreeIter& iter)
{
    int spos, epos;
    get_selection_bounds(spos, epos);

    size_t pos{ get_text().substr(0, spos).find_last_of(' ') };
    std::string prefix{ pos == std::string::npos ? (get_text()[0] == '-' ? "-" : "")
                                                 : get_text().substr(0, pos + 1) },
        tag{ iter->get_value(m_Columns.tag) }, suffix{ get_text().substr(epos) };

    if (pos != std::string::npos && get_text().substr(pos + 1, 1) == "-")
        prefix += '-';

    set_text(prefix + tag + " " + suffix);

    select_region(0, 0);
    set_position(epos + 1);

    return true;
}

// Sets the forground color of the cell based on the tag type
void TagEntry::on_tag_cell_data(const Gtk::TreeIter& iter)
{
    Gtk::CellRenderer* c{ m_TagCompletion->get_first_cell() };
    auto* cell{ static_cast<Gtk::CellRendererText*>(c) };
    auto tag{ iter->get_value(m_Columns.tag) };

    cell->property_text().set_value(tag);

    if (tag.type != Tag::Type::GENERAL && tag.type != Tag::Type::UNKNOWN)
        cell->property_foreground_rgba().set_value(tag);

    cell->property_foreground_set().set_value(tag.type != Tag::Type::GENERAL &&
                                              tag.type != Tag::Type::DEPRECATED &&
                                              tag.type != Tag::Type::UNKNOWN);
}
