#ifndef _TAGVIEW_H_
#define _TAGVIEW_H_

#include "tagentry.h"

namespace AhoViewer
{
    namespace Booru
    {
        class TagView : public Gtk::TreeView
        {
        public:
            TagView(BaseObjectType *cobj, const Glib::RefPtr<Gtk::Builder>&);
            ~TagView();

            void clear() { m_ListStore->clear(); }

            void set_tags(const std::set<std::string> &tags);
            void set_tag_entry(TagEntry *e) { m_TagEntry = e; }
        protected:
            virtual bool on_button_press_event(GdkEventButton *e);
        private:
            class ModelColumns : public Gtk::TreeModel::ColumnRecord
            {
            public:
                ModelColumns() { add(tag_column); }
                Gtk::TreeModelColumn<std::string> tag_column;
            };

            void on_cell_data(Gtk::CellRenderer *c, const Gtk::TreeModel::iterator &iter);

            Glib::RefPtr<Gtk::ListStore> m_ListStore;
            TagEntry *m_TagEntry;
        };
    }
}

#endif /* _TAGVIEW_H_ */
