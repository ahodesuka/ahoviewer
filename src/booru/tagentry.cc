#include "tagentry.h"
using namespace AhoViewer::Booru;

TagEntry::TagEntry(BaseObjectType *cobj, const Glib::RefPtr<Gtk::Builder> &bldr)
  : Gtk::Entry(cobj)
{
    m_TagCompletion = Glib::RefPtr<Gtk::EntryCompletion>::cast_static(
            bldr->get_object("Booru::Browser::TagEntryCompletion"));

    m_Model = Gtk::ListStore::create(m_Columns);
    set_completion(m_TagCompletion);

    m_TagCompletion->set_match_func(sigc::mem_fun(*this, &TagEntry::match_func));
    m_TagCompletion->set_model(m_Model);
    m_TagCompletion->set_text_column(m_Columns.tag_column);
    m_TagCompletion->signal_match_selected().connect(sigc::mem_fun(*this, &TagEntry::on_match_selected), false);

    // XXX: gtkmm's cursor-on-match signal seems to be broken.
    g_signal_connect(m_TagCompletion->gobj(), "cursor-on-match", G_CALLBACK(on_cursor_on_match_c), this);

    m_ChangedConn = signal_changed().connect(sigc::mem_fun(*this, &TagEntry::on_text_changed));
}

void TagEntry::set_tags(const std::set<std::string> &tags)
{
    m_Tags = &tags;
}

void TagEntry::on_grab_focus()
{
    Gtk::Entry::on_grab_focus();

    select_region(0, 0);
    set_position(-1);
}

void TagEntry::on_text_changed()
{
    int spos = get_position();

    size_t pos = get_text().substr(0, spos).find_last_of(' ');
    std::string key = pos == std::string::npos ?
        get_text().substr(0, spos + 1) : get_text().substr(pos + 1, spos - pos);

    if (key.back() == ' ')
        key.pop_back();

    // Check if key is prefixed
    if (key.front() == '-')
        key.erase(key.begin());

    m_Model->clear();

    if (key.length() >= static_cast<size_t>(m_TagCompletion->get_minimum_key_length()))
    {
        size_t i = 0;
        for (const std::string &tag : *m_Tags)
        {
            if (tag.compare(0, key.length(), key) == 0)
            {
                Gtk::TreeIter iter = m_Model->append();
                iter->set_value(m_Columns.tag_column, tag);

                // Limit list to 20 tags
                if (++i >= 20)
                    break;
            }
        }
    }

    m_TagCompletion->complete();
}

gboolean TagEntry::on_cursor_on_match_c(GtkEntryCompletion*,
        GtkTreeModel *model, GtkTreeIter *iter, TagEntry *entry)
{
    GValue value = G_VALUE_INIT;
    gtk_tree_model_get_value(model, iter, 0, &value);
    entry->on_cursor_on_match(static_cast<const char*>(g_value_get_string(&value)));
    g_value_unset(&value);

    return TRUE;
}

void TagEntry::on_cursor_on_match(const std::string &tag)
{
    int spos, epos;
    get_selection_bounds(spos, epos);

    size_t pos = get_text().substr(0, spos).find_last_of(' ');
    std::string prefix = pos == std::string::npos ?
        (get_text()[0] == '-' ? "-" : "") : get_text().substr(0, pos + 1),
                suffix = get_text().substr(epos);

    if (pos != std::string::npos && get_text().substr(pos + 1, 1) == "-")
        prefix += '-';

    m_ChangedConn.block();
    set_text(prefix + tag + suffix);
    m_ChangedConn.unblock();

    // This is used to keep track of what part of the tag was completed
    select_region(spos, tag.size() - (spos - prefix.size()) + spos);
}

bool TagEntry::on_match_selected(const Gtk::TreeIter &iter)
{
    int spos, epos;
    get_selection_bounds(spos, epos);

    size_t pos = get_text().substr(0, spos).find_last_of(' ');
    std::string prefix = pos == std::string::npos ?
        (get_text()[0] == '-' ? "-" : "") : get_text().substr(0, pos + 1),
                tag    = iter->get_value(m_Columns.tag_column),
                suffix = get_text().substr(epos);

    if (pos != std::string::npos && get_text().substr(pos + 1, 1) == "-")
        prefix += '-';

    m_ChangedConn.block();
    set_text(prefix + tag + " " + suffix);
    m_ChangedConn.unblock();

    select_region(0, 0);
    set_position(epos + 1);

    return true;
}
