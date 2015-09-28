#include "tagview.h"
using namespace AhoViewer::Booru;

#include "settings.h"

// {{{ Favorite Icons
/**
 * These icons are from http://raphaeljs.com/icons
 *
 * Copyright © 2008 Dmitry Baranovskiy
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the “Software”), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * The software is provided “as is”, without warranty of any kind, express or
 * implied, including but not limited to the warranties of merchantability, fitness
 * for a particular purpose and noninfringement. In no event shall the authors or
 * copyright holders be liable for any claim, damages or other liability, whether
 * in an action of contract, tort or otherwise, arising from, out of or in
 * connection with the software or the use or other dealings in the software.
 **/
const std::string TagView::StarSVG = "<?xml version=\"1.0\" standalone=\"no\"?>\
<svg width=\"16px\" height=\"16px\" viewBox=\"-2 0 32 32\" version=\"1.1\" xmlns=\"http://www.w3.org/2000/svg\">\
  <path fill=\"%1\" d=\"M22.441,28.181c-0.419,0-0.835-0.132-1.189-0.392l-5.751-4.247L9.75,\
  27.789c-0.354,0.26-0.771,0.392-1.189,0.392c-0.412,\
  0-0.824-0.128-1.175-0.384c-0.707-0.511-1-1.422-0.723-2.25l2.26-6.783l-5.815-4.158c-0.71-0.509-1.009-1.416-0.74-2.246c0.268-0.826,\
  1.037-1.382,1.904-1.382c0.004,0,0.01,0,0.014,0l7.15,0.056l2.157-6.816c0.262-0.831,1.035-1.397,\
  1.906-1.397s1.645,0.566,1.906,1.397l2.155,6.816l7.15-0.056c0.004,0,\
  0.01,0,0.015,0c0.867,0,1.636,0.556,1.903,1.382c0.271,0.831-0.028,\
  1.737-0.739,2.246l-5.815,4.158l2.263,6.783c0.276,0.826-0.017,1.737-0.721,\
  2.25C23.268,28.053,22.854,28.181,22.441,28.181L22.441,28.181z\"/>\
</svg>";

const std::string TagView::StarOutlineSVG = "<?xml version=\"1.0\" standalone=\"no\"?>\
<svg width=\"16px\" height=\"16px\" viewBox=\"-2 0 32 32\" version=\"1.1\" xmlns=\"http://www.w3.org/2000/svg\">\
  <path fill=\"%1\" d=\"M28.631,12.359c-0.268-0.826-1.036-1.382-1.903-1.382h-0.015l-7.15,\
  0.056l-2.155-6.816c-0.262-0.831-1.035-1.397-1.906-1.397s-1.645,0.566-1.906,1.397l-2.157,\
  6.816l-7.15-0.056H4.273c-0.868,0-1.636,0.556-1.904,1.382c-0.27,0.831,0.029,1.737,0.74,\
  2.246l5.815,4.158l-2.26,6.783c-0.276,0.828,0.017,1.739,0.723,2.25c0.351,0.256,0.763,0.384,\
  1.175,0.384c0.418,0,0.834-0.132,1.189-0.392l5.751-4.247l5.751,4.247c0.354,0.26,0.771,0.392,\
  1.189,0.392c0.412,0,0.826-0.128,1.177-0.384c0.704-0.513,0.997-1.424,\
  0.721-2.25l-2.263-6.783l5.815-4.158C28.603,14.097,28.901,13.19,28.631,12.359zM19.712,\
  17.996l2.729,8.184l-6.94-5.125L8.56,26.18l2.729-8.184l-7.019-5.018l8.627,0.066L15.5,\
  4.82l2.603,8.225l8.627-0.066L19.712,17.996z\"/>\
</svg>";
// }}}

TagView::TagView(BaseObjectType *cobj, const Glib::RefPtr<Gtk::Builder> &bldr)
  : Gtk::TreeView(cobj)
{
    bldr->get_widget_derived("Booru::Browser::TagEntry", m_TagEntry);
    m_TagEntry->signal_changed().connect([ this ]() { queue_draw(); });

    m_ListStore = Gtk::ListStore::create(m_Columns);

    set_model(m_ListStore);

    Gtk::CellRendererPixbuf *fcell = Gtk::manage(new Gtk::CellRendererPixbuf());
    append_column("Favorite", *fcell);

    Gtk::CellRendererToggle *cell = Gtk::manage(new Gtk::CellRendererToggle());
    append_column("Toggle", *cell);

    append_column("Tag", m_Columns.tag);

    get_column(0)->set_cell_data_func(*fcell, sigc::mem_fun(*this, &TagView::on_favorite_cell_data));
    get_column(1)->set_cell_data_func(*cell, sigc::mem_fun(*this, &TagView::on_toggle_cell_data));

    m_FavoriteTags = &Settings.get_favorite_tags();
    show_favorite_tags();
}

void TagView::set_tags(const std::set<std::string> &tags)
{
    clear();
    for (const std::string &tag : tags)
        m_ListStore->append()->set_value(m_Columns.tag, tag);

    if (get_realized())
        scroll_to_point(0, 0);
}

void TagView::on_style_changed(const Glib::RefPtr<Gtk::Style> &s)
{
    Gtk::TreeView::on_style_changed(s);

    m_PrevColor = m_Color;
    m_Color = get_style()->get_bg(Gtk::STATE_SELECTED);

    if (m_PrevColor != m_Color)
        update_favorite_icons();
}

bool TagView::on_button_press_event(GdkEventButton *e)
{
    if (e->button == 1)
    {
        Gtk::TreePath path;
        get_path_at_pos(e->x, e->y, path);

        if (path)
        {
            std::string tag = m_ListStore->get_iter(path)->get_value(m_Columns.tag);

            // The favorite column was clicked
            if (e->x < get_column(0)->get_width())
            {
                if (m_FavoriteTags->find(tag) != m_FavoriteTags->end())
                    Settings.remove_favorite_tag(tag);
                else
                    Settings.add_favorite_tag(tag);
            }
            else
            {
                // Open tag alone in new tab
                if ((e->state & GDK_SHIFT_MASK) == GDK_SHIFT_MASK)
                {
                    m_SignalNewTabTag(tag);
                }
                else
                {
                    std::istringstream ss(m_TagEntry->get_text());
                    std::set<std::string> tags = { std::istream_iterator<std::string>(ss),
                                                   std::istream_iterator<std::string>() };

                    if (tags.find(tag) != tags.end())
                        tags.erase(tag);
                    else
                        tags.insert(tags.end(), tag);

                    std::ostringstream oss;
                    std::copy(tags.begin(), tags.end(), std::ostream_iterator<std::string>(oss, " "));
                    m_TagEntry->set_text(oss.str());
                    m_TagEntry->set_position(-1);
                }
            }

            queue_draw();
        }
    }

    return true;
}

void TagView::on_favorite_cell_data(Gtk::CellRenderer *c, const Gtk::TreeIter &iter)
{
    Gtk::CellRendererPixbuf *cell = static_cast<Gtk::CellRendererPixbuf*>(c);
    std::string tag = iter->get_value(m_Columns.tag);

    if (m_FavoriteTags->find(tag) != m_FavoriteTags->end())
        cell->property_pixbuf().set_value(m_StarPixbuf);
    else
        cell->property_pixbuf().set_value(m_StarOutlinePixbuf);
}

void TagView::on_toggle_cell_data(Gtk::CellRenderer *c, const Gtk::TreeIter &iter)
{
    Gtk::CellRendererToggle *cell = static_cast<Gtk::CellRendererToggle*>(c);
    std::istringstream ss(m_TagEntry->get_text());
    std::set<std::string> tags = { std::istream_iterator<std::string>(ss),
                                   std::istream_iterator<std::string>() };

    std::string tag = iter->get_value(m_Columns.tag);
    cell->set_active(std::find(tags.begin(), tags.end(), tag) != tags.end());
}

void TagView::update_favorite_icons()
{
    Glib::RefPtr<Gdk::PixbufLoader> loader;
    Glib::ustring vgdata;
    gchar *color = g_strdup_printf("#%02x%02x%02x", (m_Color.get_red()   >> 8) & 0xff,
                                                    (m_Color.get_green() >> 8) & 0xff,
                                                    (m_Color.get_blue()  >> 8) & 0xff);

    loader = Gdk::PixbufLoader::create("svg");
    vgdata = Glib::ustring::compose(StarSVG, color);
    loader->write(reinterpret_cast<const unsigned char*>(vgdata.c_str()), vgdata.size());
    loader->close();
    m_StarPixbuf = loader->get_pixbuf();

    loader = Gdk::PixbufLoader::create("svg");
    vgdata = Glib::ustring::compose(StarOutlineSVG, color);
    loader->write(reinterpret_cast<const unsigned char*>(vgdata.c_str()), vgdata.size());
    loader->close();
    m_StarOutlinePixbuf = loader->get_pixbuf();

    g_free(color);
}
