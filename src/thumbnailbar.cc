#include <algorithm>

#include "thumbnailbar.h"
using namespace AhoViewer;

#include "image.h"

ThumbnailBar::ThumbnailBar(BaseObjectType *cobj, const Glib::RefPtr<Gtk::Builder> &bldr)
  : Gtk::ScrolledWindow(cobj)
{
    ModelColumns columns;

    bldr->get_widget("ThumbnailBar::TreeView", m_TreeView);

    m_VAdjust = Glib::RefPtr<Gtk::Adjustment>::cast_static(bldr->get_object("ThumbnailBar::VAdjust"));

    m_ListStore = Gtk::ListStore::create(columns);
    m_TreeView->set_model(m_ListStore);
    m_TreeView->append_column("Thumbnail", columns.pixbuf_column);
    m_TreeView->set_size_request(Image::ThumbnailSize + 9, -1);
    m_TreeView->signal_cursor_changed().connect(sigc::mem_fun(*this, &ThumbnailBar::on_cursor_changed));
}

ThumbnailBar::~ThumbnailBar()
{

}

void ThumbnailBar::set_selected(const size_t index)
{
    if (get_window())
    {
        get_window()->freeze_updates();

        Gtk::TreePath path(std::to_string(index));
        Gtk::TreeViewColumn *column = m_TreeView->get_column(0);
        Gdk::Rectangle rect;

        // Make sure everything is updated before getting scroll position
        while (Gtk::Main::events_pending())
            Gtk::Main::iteration();

        // Center the selected thumbnail
        m_TreeView->get_background_area(path, *column, rect);
        double value = m_VAdjust->get_value() + rect.get_y() +
            (rect.get_height() / 2) - (m_VAdjust->get_page_size() / 2);
        value = std::min(std::max(value, 0.0), m_VAdjust->get_upper() - m_VAdjust->get_page_size());
        m_VAdjust->set_value(value);

        m_TreeView->get_selection()->select(path);

        get_window()->thaw_updates();
    }
}

void ThumbnailBar::on_cursor_changed()
{
    Gtk::TreePath path;
    Gtk::TreeViewColumn *column;

    m_TreeView->get_cursor(path, column);
    m_SignalSelectedChanged(path[0]);
}
