#include "tagview.h"
using namespace AhoViewer::Booru;

TagView::TagView(BaseObjectType *cobj, const Glib::RefPtr<Gtk::Builder>&)
  : Gtk::TreeView(cobj)
{
    ModelColumns columns;
    m_ListStore = Gtk::ListStore::create(columns);

    set_model(m_ListStore);

    Gtk::CellRendererToggle *cell = Gtk::manage(new Gtk::CellRendererToggle());
    append_column("Toggle", *cell);
    append_column("Tag", columns.tag_column);
    get_column(0)->set_cell_data_func(*cell, sigc::mem_fun(*this, &TagView::on_cell_data));
}

TagView::~TagView()
{

}

bool TagView::on_button_press_event(GdkEventButton *e)
{
    if (e->button == 1)
    {
        Gtk::TreePath path;
        get_path_at_pos(e->x, e->y, path);

        if (path)
        {
            std::string tag;
            std::istringstream ss(m_TagEntry->get_text());
            std::set<std::string> tags = { std::istream_iterator<std::string>(ss),
                                           std::istream_iterator<std::string>() };
            Gtk::TreeIter iter = m_ListStore->get_iter(path);

            iter->get_value(0, tag);

            if (std::find(tags.begin(), tags.end(), tag) != tags.end())
                tags.erase(tag);
            else
                tags.insert(tag);

            std::ostringstream oss;
            std::copy(tags.begin(), tags.end(), std::ostream_iterator<std::string>(oss, " "));
            m_TagEntry->set_text(oss.str());
        }
    }

    return Gtk::TreeView::on_button_press_event(e);
}

void TagView::on_cell_data(Gtk::CellRenderer *c, const Gtk::TreeIter &iter)
{
    Gtk::CellRendererToggle *cell = static_cast<Gtk::CellRendererToggle*>(c);
    std::string tag;
    std::istringstream ss(m_TagEntry->get_text());
    std::set<std::string> tags = { std::istream_iterator<std::string>(ss),
                                   std::istream_iterator<std::string>() };

    iter->get_value(0, tag);
    cell->set_active(std::find(tags.begin(), tags.end(), tag) != tags.end());
}

void TagView::set_tags(const std::set<std::string> &tags)
{
    clear();
    for (const std::string &tag : tags)
        m_ListStore->append()->set_value(0, tag);
}
