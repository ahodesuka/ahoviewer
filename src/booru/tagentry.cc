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
    size_t pos = get_text().find_last_of(' ');

    int spos, epos;
    get_selection_bounds(spos, epos);

    std::string key = pos == std::string::npos ?
        get_text() : get_text().substr(pos + 1, spos - pos + 1);

    // Check if key is prefixed
    if (key[0] == '-')
        key = key.substr(1);

    Gtk::TreeRow row;
    m_Model->clear();

    if (key.length() >= static_cast<size_t>(m_TagCompletion->get_minimum_key_length()))
    {
        size_t i = 0;
        for (const std::string &tag : *m_Tags)
        {
            if (tag.compare(0, key.length(), key) == 0)
            {
                row = *(m_Model->append());
                row[m_Columns.tag_column] = tag;

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
    size_t pos = get_text().find_last_of(' ');
    std::string tags = pos == std::string::npos ? "" : get_text().substr(0, pos + 1),
                prefix = (pos == std::string::npos ?
                            get_text()[0] : get_text()[pos + 1]) == '-' ? "-" : "";

    int spos, epos;
    get_selection_bounds(spos, epos);

    m_ChangedConn.block();
    set_text(tags + prefix + tag);
    m_ChangedConn.unblock();

    select_region(spos, -1);
}

bool TagEntry::on_match_selected(const Gtk::TreeIter &iter)
{
    std::string tag((*iter)->get_value(m_Columns.tag_column));
    size_t pos = get_text().find_last_of(' ');
    std::string tags = pos == std::string::npos ? "" : get_text().substr(0, pos + 1),
                prefix = (pos == std::string::npos ?
                            get_text()[0] : get_text()[pos + 1]) == '-' ? "-" : "";

    m_ChangedConn.block();
    set_text(tags + prefix + tag + " ");
    m_ChangedConn.unblock();

    select_region(0, 0);
    set_position(-1);

    return true;
}
