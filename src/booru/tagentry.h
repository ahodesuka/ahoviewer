#ifndef _TAGENTRY_H_
#define _TAGENTRY_H_

#include <gtkmm.h>
#include <set>

namespace AhoViewer
{
    namespace Booru
    {
        class TagEntry : public Gtk::Entry
        {
        public:
            TagEntry(BaseObjectType *cobj, const Glib::RefPtr<Gtk::Builder> &bldr);
            virtual ~TagEntry() override = default;

            void set_tags(const std::set<std::string> &tags);
        protected:
            virtual void on_grab_focus() override;
        private:
            struct ModelColumns : public Gtk::TreeModelColumnRecord
            {
                ModelColumns() { add(tag_column); }
                Gtk::TreeModelColumn<std::string> tag_column;
            };

            inline bool match_func(const std::string&,
                                   const Gtk::TreeModel::const_iterator&) { return true; }

            void on_text_changed();
            static gboolean on_cursor_on_match_c(GtkEntryCompletion*, GtkTreeModel*,
                                                 GtkTreeIter*, TagEntry*);
            void on_cursor_on_match(const std::string &tag);
            bool on_match_selected(const Gtk::TreeIter &iter);

            // These are the tags used for completion
            const std::set<std::string> *m_Tags;
            Glib::RefPtr<Gtk::EntryCompletion> m_TagCompletion;
            Glib::RefPtr<Gtk::ListStore> m_Model;
            ModelColumns m_Columns;

            sigc::connection m_ChangedConn;
        };
    }
}

#endif /* _TAGENTRY_H_ */
