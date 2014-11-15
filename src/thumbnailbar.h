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
        virtual ~ThumbnailBar();

        virtual void on_thumbnails_loaded(const size_t index) { set_selected(index); }
        virtual void set_selected(const size_t index);
    private:
        void on_cursor_changed();

        Gtk::TreeView *m_TreeView;
        Glib::RefPtr<Gtk::Adjustment> m_VAdjust;
    };
}

#endif /* _THUMBNAILBAR_H_ */
