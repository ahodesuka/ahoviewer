#include "tagview.h"
using namespace AhoViewer::Booru;

#include "settings.h"

#include <glibmm/i18n.h>
#include <tuple>

// {{{ Favorite Icons
/*
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
 */
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

// Matches class names in tagview-colors.css
static std::string tag_type_to_css_class(const Tag::Type t)
{
    switch (t)
    {
    case Tag::Type::ARTIST:
        return "artist";
    case Tag::Type::CHARACTER:
        return "character";
    case Tag::Type::COPYRIGHT:
        return "copyright";
    case Tag::Type::METADATA:
        return "metadata";
    case Tag::Type::GENERAL:
        return "general";
    case Tag::Type::DEPRECATED:
        return "deprecated";
    default:
        return "";
    }
}

// Used for header labels (translatable)
static Glib::ustring tag_type_to_string(const Tag::Type t)
{
    switch (t)
    {
    case Tag::Type::ARTIST:
        return _("Artist");
    case Tag::Type::CHARACTER:
        return _("Character");
    case Tag::Type::COPYRIGHT:
        return _("Copyright");
    case Tag::Type::METADATA:
        return _("Metadata");
    case Tag::Type::GENERAL:
        return _("General");
    case Tag::Type::DEPRECATED:
        return _("Deprecated");
    default:
        return "";
    }
}

TagView::Row::Row(const Tag& t)
    : Gtk::ListBoxRow{},
      tag{ t },
      check_button{ t },
      m_Box{ Gtk::ORIENTATION_HORIZONTAL, 2 }
{
    set_name("TagViewRow");

    m_EBox.add(m_Image);
    m_Box.pack_start(check_button, true, true, 4);
    m_Box.pack_start(m_EBox, false, false, 4);

    static_cast<Gtk::Label*>(check_button.get_child())->set_ellipsize(Pango::ELLIPSIZE_END);
    check_button.add_events(Gdk::BUTTON_PRESS_MASK);
    check_button.signal_button_press_event().connect(
        [&](GdkEventButton* e) { return m_SignalTagButtonPressed(e, this); }, false);
    m_EBox.signal_button_press_event().connect([&](GdkEventButton* e) {
        if (e->type == GDK_BUTTON_PRESS && e->button == 1)
        {
            set_favorite(m_Favorite = !m_Favorite);
            m_SignalTagFavToggled(this);
            return true;
        }

        return false;
    });

    get_style_context()->add_class(tag_type_to_css_class(tag.type));

    add(m_Box);
    show_all();
}

TagView::Header::Header(const Tag::Type t) : Gtk::Box{ Gtk::ORIENTATION_HORIZONTAL }
{
    m_Label.set_label(tag_type_to_string(t));
    m_Label.set_xalign(0.0);

    set_name("TagViewHeader");
    get_style_context()->add_class(tag_type_to_css_class(t));

    m_Separator.set_valign(Gtk::ALIGN_CENTER);
    pack_start(m_Label, false, false, 16);
    pack_start(m_Separator, true, true);

    show_all();
}

TagView::TagView(BaseObjectType* cobj, const Glib::RefPtr<Gtk::Builder>& bldr)
    : Gtk::ListBox{ cobj },
      m_FavoriteTags{ Settings.get_favorite_tags() }
{
    auto model{ Glib::RefPtr<Gio::Menu>::cast_dynamic(bldr->get_object("TagViewPopoverMenu")) };
    m_PopupMenu = Gtk::make_managed<Gtk::Menu>(model);
    m_PopupMenu->attach_to_widget(*this);

    bldr->get_widget_derived("Booru::Browser::TagEntry", m_TagEntry);
    bldr->get_widget("Booru::Browser::TagView::ScrolledWindow", m_ScrolledWindow);

    m_TagEntry->signal_changed().connect(sigc::mem_fun(*this, &TagView::on_entry_value_changed));

    auto css{ Gtk::CssProvider::create() }, css_colors{ Gtk::CssProvider::create() };
    auto style_context{ get_style_context() };
    style_context->add_provider_for_screen(
        Gdk::Screen::get_default(), css, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    css->load_from_resource("/ui/css/tagview.css");

    gsize size;
    // This is pretty ugly, but the order of colors is the same as the order of the
    // Tag::Type enum entries
    static const auto css_data{ Glib::ustring::compose(
        Glib::ustring(reinterpret_cast<const char*>(
            Gio::Resource::lookup_data_global("/ui/css/tagview-colors.css")->get_data(size))),
        Settings.get_string("TagArtistColor"),
        Settings.get_string("TagCharacterColor"),
        Settings.get_string("TagCopyrightColor"),
        Settings.get_string("TagMetadataColor")) };
    style_context->add_provider_for_screen(
        Gdk::Screen::get_default(), css_colors, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    css_colors->load_from_data(css_data);
}

void TagView::clear()
{
    for (auto w : get_children())
        remove(*w);

    m_Tags = nullptr;
}

void TagView::set_tags(std::vector<Tag>& tags, const double pos)
{
    if (m_Tags == &tags)
        return;

    unset_sort_func();
    clear();
    m_Tags = &tags;

    std::istringstream ss(m_TagEntry->get_text());
    std::vector<std::string> entry_tags{ std::istream_iterator<std::string>{ ss },
                                         std::istream_iterator<std::string>{} };

    // Loop through tags and make rows..
    for (auto t : *m_Tags)
    {
        auto row{ Gtk::make_managed<Row>(t) };

        row->signal_tag_button_press_event().connect(
            sigc::mem_fun(*this, &TagView::on_tag_button_press_event));
        row->signal_tag_favorite_toggled().connect(
            sigc::mem_fun(*this, &TagView::on_tag_favorite_toggled));

        row->check_button.set_active(
            std::find(entry_tags.begin(), entry_tags.end(), row->tag.tag) != entry_tags.end());

        add(*row);
        row->set_favorite(m_Tags == &m_FavoriteTags ||
                          std::find(m_FavoriteTags.begin(), m_FavoriteTags.end(), t) !=
                              m_FavoriteTags.end());
    }

    set_sort_order(Settings.get_tag_view_order());
    m_ScrolledWindow->get_vadjustment()->set_value(pos);
}

void TagView::set_sort_order(const TagViewOrder& order)
{
    Settings.set_tag_view_order(order);

    if (Settings.get_tag_view_order() == TagViewOrder::TAG)
    {
        static const auto sort_by_tag = [](Gtk::ListBoxRow* a, Gtk::ListBoxRow* b) {
            auto ra{ static_cast<Row*>(a) }, rb{ static_cast<Row*>(b) };
            return ra->tag < rb->tag ? -1 : (ra->tag > rb->tag ? 1 : 0);
        };
        set_sort_func(sort_by_tag);
    }
    else
    {
        static const auto sort_by_type = [](Gtk::ListBoxRow* a, Gtk::ListBoxRow* b) {
            auto ra{ static_cast<Row*>(a) }, rb{ static_cast<Row*>(b) };
            return std::tie(ra->tag.type, ra->tag.tag) < std::tie(rb->tag.type, rb->tag.tag)
                       ? -1
                       : (std::tie(ra->tag.type, ra->tag.tag) > std::tie(rb->tag.type, rb->tag.tag)
                              ? 1
                              : 0);
        };
        set_sort_func(sort_by_type);
    }

    invalidate_headers();
}

void TagView::on_toggle_show_headers(bool state)
{
    Settings.set("ShowTagTypeHeaders", state);

    if (state)
        set_header_func(sigc::mem_fun(*this, &TagView::header_func));
    else
        unset_header_func();
}

void TagView::on_realize()
{
    Gtk::ListBox::on_realize();

    // Can this be run in the constructor?
    set_sort_order(Settings.get_tag_view_order());

    if (Settings.get_bool("ShowTagTypeHeaders"))
        set_header_func(sigc::mem_fun(*this, &TagView::header_func));

    show_favorite_tags();
}

void TagView::on_style_updated()
{
    m_PrevColor = m_Color;
    if (get_style_context()->lookup_color("theme_selected_bg_color", m_Color) &&
        m_PrevColor != m_Color)
        update_favorite_icons();
}

bool TagView::on_button_press_event(GdkEventButton* e)
{
    if (e->button == 3)
    {
        m_PopupMenu->popup_at_pointer(reinterpret_cast<GdkEvent*>(e));
        return true;
    }

    return Gtk::ListBox::on_button_press_event(e);
}

void TagView::on_entry_value_changed()
{
    std::istringstream ss(m_TagEntry->get_text());
    std::vector<std::string> tags{ std::istream_iterator<std::string>{ ss },
                                   std::istream_iterator<std::string>{} };

    for (auto w : get_children())
    {
        auto row{ static_cast<Row*>(w) };
        row->check_button.set_active(std::find(tags.begin(), tags.end(), row->tag.tag) !=
                                     tags.end());
    }
}

bool TagView::on_tag_button_press_event(GdkEventButton* e, Row* row)
{
    if (e->type != GDK_BUTTON_PRESS)
        return false;

    // Shift+left click or middle mouse click to open tag in new tab
    if ((e->button == 1 && (e->state & GDK_SHIFT_MASK) == GDK_SHIFT_MASK) || e->button == 2)
    {
        m_SignalNewTabTag(row->tag.tag + " ");
        return true;
    }
    else if (e->button == 1)
    {
        row->check_button.set_active(!row->check_button.get_active());

        std::istringstream ss(m_TagEntry->get_text());
        std::vector<std::string> tags{ std::istream_iterator<std::string>{ ss },
                                       std::istream_iterator<std::string>{} };

        if (std::find(tags.begin(), tags.end(), row->tag.tag) != tags.end())
            tags.erase(std::remove(tags.begin(), tags.end(), row->tag.tag), tags.end());
        else
            tags.push_back(row->tag);

        std::ostringstream oss;
        std::copy(tags.begin(), tags.end(), std::ostream_iterator<std::string>(oss, " "));
        m_TagEntry->set_text(oss.str());
        m_TagEntry->set_position(-1);

        return true;
    }

    return false;
}

void TagView::on_tag_favorite_toggled(Row* row)
{
    if (std::find(m_FavoriteTags.begin(), m_FavoriteTags.end(), row->tag) != m_FavoriteTags.end())
        Settings.remove_favorite_tag(row->tag);
    else
        Settings.add_favorite_tag(row->tag);
}

void TagView::header_func(Gtk::ListBoxRow* a, Gtk::ListBoxRow* b)
{
    if (Settings.get_tag_view_order() == TagViewOrder::TAG)
    {
        a->unset_header();
        return;
    }

    auto ra{ static_cast<Row*>(a) };
    if (ra->tag.type == Tag::Type::UNKNOWN)
        return;

    if (!b)
    {
        auto h{ Gtk::make_managed<Header>(ra->tag.type) };
        ra->set_header(*h);
    }
    else
    {
        auto rb{ static_cast<Row*>(b) };
        if (ra->tag.type != rb->tag.type)
        {
            auto h{ Gtk::make_managed<Header>(ra->tag.type) };
            ra->set_header(*h);
        }
        else
        {
            ra->unset_header();
        }
    }
}

void TagView::update_favorite_icons()
{
    Glib::RefPtr<Gdk::PixbufLoader> loader;
    std::string vgdata;

    loader = Gdk::PixbufLoader::create("svg");
    vgdata = Glib::ustring::compose(StarSVG, m_Color.to_string());
    loader->write(reinterpret_cast<const unsigned char*>(vgdata.c_str()), vgdata.size());
    loader->close();
    m_StarPixbuf = loader->get_pixbuf();

    loader = Gdk::PixbufLoader::create("svg");
    vgdata = Glib::ustring::compose(StarOutlineSVG, m_Color.to_string());
    loader->write(reinterpret_cast<const unsigned char*>(vgdata.c_str()), vgdata.size());
    loader->close();
    m_StarOutlinePixbuf = loader->get_pixbuf();
}
