#include "thumbnailbar.h"
using namespace AhoViewer;

#include "image.h"

#include <algorithm>

ThumbnailBar::ThumbnailBar(BaseObjectType* cobj, const Glib::RefPtr<Gtk::Builder>& bldr)
    : Gtk::ScrolledWindow(cobj)
{
    bldr->get_widget("ThumbnailBar::TreeView", m_TreeView);
    m_UIManager = Glib::RefPtr<Gtk::UIManager>::cast_static(bldr->get_object("UIManager"));

    m_VAdjust =
        Glib::RefPtr<Gtk::Adjustment>::cast_static(bldr->get_object("ThumbnailBar::VAdjust"));

    m_TreeView->set_model(m_ListStore);
    m_TreeView->append_column("Thumbnail", m_Columns.pixbuf);
    m_TreeView->set_size_request(Image::ThumbnailSize + 9, -1);
    m_CursorConn = m_TreeView->signal_cursor_changed().connect(
        sigc::mem_fun(*this, &ThumbnailBar::on_cursor_changed));
    m_TreeView->signal_button_press_event().connect(
        sigc::mem_fun(*this, &ThumbnailBar::on_button_press_event), false);

    // If the user scrolls the widget, this will keep scroll_to_selected from being
    // called when thumbnails are being loaded
    m_ScrollConn =
        get_vadjustment()->signal_value_changed().connect([&]() { m_KeepAligned = false; });
}

void ThumbnailBar::clear()
{
    ImageList::Widget::clear();
    m_KeepAligned = true;
}

void ThumbnailBar::set_pixbuf(const size_t index, const Glib::RefPtr<Gdk::Pixbuf>& pixbuf)
{
    m_ScrollConn.block();
    ImageList::Widget::set_pixbuf(index, pixbuf);
    while (Glib::MainContext::get_default()->pending())
        Glib::MainContext::get_default()->iteration(true);
    m_ScrollConn.unblock();

    // Keep the selected image centered while thumbnails are being added
    if (m_KeepAligned && get_window())
    {
        get_window()->freeze_updates();

        Gtk::TreePath path{ m_TreeView->get_selection()->get_selected() };
        Gtk::TreeViewColumn* column{ m_TreeView->get_column(0) };
        Gdk::Rectangle rect;

        // Center the selected thumbnail
        m_TreeView->get_background_area(path, *column, rect);
        double value = m_VAdjust->get_value() + rect.get_y() + (rect.get_height() / 2) -
                       (m_VAdjust->get_page_size() / 2);
        value =
            std::round(std::clamp(value, 0.0, m_VAdjust->get_upper() - m_VAdjust->get_page_size()));

        m_ScrollConn.block();
        m_VAdjust->set_value(value);
        m_ScrollConn.unblock();

        get_window()->thaw_updates();
    }
}

void ThumbnailBar::on_realize()
{
    Gtk::ScrolledWindow::on_realize();

    m_PopupMenu = static_cast<Gtk::Menu*>(m_UIManager->get_widget("/ThumbnailBarPopupMenu"));
}

void ThumbnailBar::on_show()
{
    Gtk::ScrolledWindow::on_show();
    scroll_to_selected();
}

void ThumbnailBar::set_selected(const size_t index)
{
    Gtk::TreePath path(std::to_string(index));
    m_TreeView->get_selection()->select(path);
    scroll_to_selected();
}

void ThumbnailBar::scroll_to_selected()
{
    if (get_window())
    {
        Gtk::TreePath path{ m_TreeView->get_selection()->get_selected() };

        m_ScrollConn.block();
        m_TreeView->scroll_to_row(path, 0.5);
        m_ScrollConn.unblock();
    }
}

bool ThumbnailBar::on_button_press_event(GdkEventButton* e)
{
    if (e->type == GDK_BUTTON_PRESS && e->button == 3)
    {
        Gtk::TreePath path;

        if (m_TreeView->get_path_at_pos(e->x, e->y, path))
        {
            m_TreeView->set_cursor(path);
            m_ScrollConn.block();
            m_TreeView->scroll_to_row(path);
            m_ScrollConn.unblock();

            m_PopupMenu->popup_at_pointer((GdkEvent*)e);

            return true;
        }
    }

    return false;
}

void ThumbnailBar::on_cursor_changed()
{
    Gtk::TreePath path;
    Gtk::TreeViewColumn* column;

    m_TreeView->get_cursor(path, column);

    if (!path)
        return;

    m_SignalSelectedChanged(path[0]);

    Gtk::TreeIter iter = m_ListStore->get_iter(path);
    if (iter)
        m_KeepAligned = !iter->get_value(m_Columns.pixbuf);
}
