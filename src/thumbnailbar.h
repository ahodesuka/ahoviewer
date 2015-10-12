#ifndef _THUMBNAILBAR_H_
#define _THUMBNAILBAR_H_

#include <gtkmm.h>

#include "imagelist.h"

namespace AhoViewer
{
    class ThumbnailBar : public Gtk::ScrolledWindow,
                         public ImageList::Widget
    {
    public:
        ThumbnailBar(BaseObjectType*, const Glib::RefPtr<Gtk::Builder>&);
        virtual ~ThumbnailBar() = default;

        virtual void clear();
        virtual void set_pixbuf(const size_t index, const Glib::RefPtr<Gdk::Pixbuf> &pixbuf);
        virtual void set_selected(const size_t index);
    protected:
        virtual void on_show();
    private:
        void scroll_to_selected();
        void on_cursor_changed();

        Gtk::TreeView *m_TreeView;
        Glib::RefPtr<Gtk::Adjustment> m_VAdjust;
        bool m_KeepAligned;
        sigc::connection m_ScrollConn;
    };
}

#endif /* _THUMBNAILBAR_H_ */
