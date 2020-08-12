#ifndef _TAGVIEW_H_
#define _TAGVIEW_H_

#include "../util.h"
#include "tagentry.h"

namespace AhoViewer::Booru
{
    class TagView : public Gtk::ListBox
    {
        using SignalNewTabTag = sigc::signal<void, const std::string&>;

        class Row : public Gtk::ListBoxRow
        {
        public:
            Row(const Tag& t);

            void set_favorite(const bool f)
            {
                auto first_set{ !m_Image.get_pixbuf() };
                auto tag_view{ static_cast<TagView*>(get_parent()) };
                if (f)
                    m_Image.set(tag_view->m_StarPixbuf);
                else
                    m_Image.set(tag_view->m_StarOutlinePixbuf);

                m_AnimConn.disconnect();
                if (!first_set)
                {
                    get_style_context()->add_class("changed");
                    m_AnimConn = Glib::signal_timeout().connect(
                        [&]() {
                            get_style_context()->remove_class("changed");
                            return false;
                        },
                        250);
                }
                m_Favorite = f;
            }

            auto signal_tag_favorite_toggled() { return m_SignalTagFavToggled; }
            auto signal_tag_button_press_event() { return m_SignalTagButtonPressed; }

            const Tag tag;
            Gtk::CheckButton check_button;

        private:
            Gtk::Box m_Box;
            Gtk::EventBox m_EBox;
            Gtk::Image m_Image;

            bool m_Favorite{ false };
            sigc::connection m_AnimConn;

            sigc::signal<void, Row*> m_SignalTagFavToggled;
            sigc::signal<bool, GdkEventButton*, Row*> m_SignalTagButtonPressed;
        };
        // For access to the star pixbufs
        friend class Row;

        class Header : public Gtk::Box
        {
        public:
            Header(const Tag::Type t);

        private:
            Gtk::Label m_Label;
            Gtk::Separator m_Separator;
            Gtk::Box m_Box;
        };

    public:
        TagView(BaseObjectType* cobj, const Glib::RefPtr<Gtk::Builder>& bldr);
        ~TagView() override = default;

        void clear();
        double get_scroll_position() const
        {
            return m_ScrolledWindow->get_vadjustment()->get_value();
        }

        void show_favorite_tags(const double pos = 0.0) { set_tags(m_FavoriteTags, pos); }
        void set_tags(std::vector<Tag>& tags, const double pos = 0.0);

        void set_sort_order(const TagViewOrder& order);
        void on_toggle_show_headers();

        SignalNewTabTag signal_new_tab_tag() const { return m_SignalNewTabTag; }

    protected:
        void on_realize() override;
        void on_style_updated() override;
        bool on_button_press_event(GdkEventButton* e) override;

    private:
        void on_entry_value_changed();
        bool on_tag_button_press_event(GdkEventButton* e, Row* row);
        void on_tag_favorite_toggled(Row* row);

        void header_func(Gtk::ListBoxRow* a, Gtk::ListBoxRow* b);
        void update_favorite_icons();

        static const std::string StarSVG, StarOutlineSVG;

        Glib::RefPtr<Gtk::UIManager> m_UIManager;
        Gtk::Menu* m_PopupMenu{ nullptr };
        Glib::RefPtr<Gtk::ToggleAction> m_ShowTagTypeHeaders;
        Glib::RefPtr<Gtk::SizeGroup> m_HeaderSizeGroup;

        TagEntry* m_TagEntry;
        Gtk::ScrolledWindow* m_ScrolledWindow;
        std::vector<Tag>*m_Tags, &m_FavoriteTags;

        Gdk::RGBA m_Color, m_PrevColor;
        Glib::RefPtr<Gdk::Pixbuf> m_StarPixbuf, m_StarOutlinePixbuf;

        SignalNewTabTag m_SignalNewTabTag;
    };
}

#endif /* _TAGVIEW_H_ */
