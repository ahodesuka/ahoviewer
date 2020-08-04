#ifndef _THUMBNAILBAR_H_
#define _THUMBNAILBAR_H_

#include "imagelist.h"

#include <gtkmm.h>

namespace AhoViewer
{
    class ThumbnailBar : public Gtk::ScrolledWindow, public ImageList::Widget
    {
    public:
        ThumbnailBar(BaseObjectType*, const Glib::RefPtr<Gtk::Builder>&);
        virtual ~ThumbnailBar() override = default;

        virtual void clear() override;
        virtual void set_pixbuf(const size_t index,
                                const Glib::RefPtr<Gdk::Pixbuf>& pixbuf) override;

    protected:
        virtual void on_show() override;

        virtual void set_selected(const size_t index) override;
        virtual void scroll_to_selected() override;

    private:
        void on_cursor_changed();

        Gtk::TreeView* m_TreeView;
        Glib::RefPtr<Gtk::Adjustment> m_VAdjust;
        bool m_KeepAligned{ true };
        sigc::connection m_ScrollConn;
    };
}

#endif /* _THUMBNAILBAR_H_ */
