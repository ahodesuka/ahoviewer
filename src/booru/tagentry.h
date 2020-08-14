#pragma once

#include "../util.h"

#include <gtkmm.h>
#include <set>

namespace AhoViewer::Booru
{
    class TagEntry : public Gtk::Entry
    {
    public:
        TagEntry(BaseObjectType* cobj, const Glib::RefPtr<Gtk::Builder>& bldr);
        ~TagEntry() override = default;

        void set_tags(const std::set<Tag>& tags);

    protected:
        void on_grab_focus() override;

    private:
        struct ModelColumns : public Gtk::TreeModelColumnRecord
        {
            ModelColumns() { add(tag); }
            Gtk::TreeModelColumn<Tag> tag;
        };

        inline bool match_func(const std::string&, const Gtk::TreeModel::const_iterator&)
        {
            return true;
        }

        void on_text_changed();
        bool on_cursor_on_match(const Gtk::TreeIter& iter);
        bool on_match_selected(const Gtk::TreeIter& iter);
        void on_tag_cell_data(const Gtk::TreeIter& iter);

        // These are the tags used for completion
        const std::set<Tag>* m_Tags;
        Glib::RefPtr<Gtk::EntryCompletion> m_TagCompletion;
        Glib::RefPtr<Gtk::ListStore> m_Model;
        ModelColumns m_Columns;

        sigc::connection m_ChangedConn;
    };
}
