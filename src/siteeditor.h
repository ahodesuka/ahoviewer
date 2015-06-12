#ifndef _SITEEDITOR_H_
#define _SITEEDITOR_H_

#include <gtkmm.h>

#include "booru/site.h"

namespace AhoViewer
{
    class SiteEditor : public Gtk::TreeView
    {
    public:
        typedef sigc::signal<void> SignalEditedType;

        SiteEditor(BaseObjectType *cobj, const Glib::RefPtr<Gtk::Builder> &bldr);
        virtual ~SiteEditor();

        SignalEditedType signal_edited() const { return m_SignalEdited; }
    private:
        class ModelColumns : public Gtk::TreeModel::ColumnRecord
        {
        public:
            ModelColumns() { add(icon); add(name); add(url); add(site); }
            Gtk::TreeModelColumn<Glib::RefPtr<Gdk::Pixbuf>> icon;
            Gtk::TreeModelColumn<std::string> name;
            Gtk::TreeModelColumn<std::string> url;
            Gtk::TreeModelColumn<std::shared_ptr<Booru::Site>> site;
        };

        void add_row();
        void delete_site();

        void on_name_edited(const std::string &p, const std::string &text);
        void on_url_edited(const std::string &p, const std::string &text);

        bool is_name_unique(const Gtk::TreeIter &iter, const std::string &name) const;
        void add_edit_site(const Gtk::TreeIter &iter);

        ModelColumns m_Columns;
        Glib::RefPtr<Gtk::ListStore> m_Model;

        Gtk::TreeView::Column *m_NameColumn,
                              *m_UrlColumn;

        std::vector<std::shared_ptr<Booru::Site>>& m_Sites;
        Glib::RefPtr<Gdk::Pixbuf> m_ErrorPixbuf;

        SignalEditedType m_SignalEdited;
    };
}

#endif /* _SITEEDITOR_H_ */
