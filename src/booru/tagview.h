#pragma once

#include "../util.h"
#include "tagentry.h"

namespace AhoViewer::Booru
{
    class TagView : public Gtk::ListBox
    {
        using SignalNewTabTagType = sigc::signal<void, const std::string&>;

        class Row : public Gtk::ListBoxRow
        {
        public:
            Row(const Tag& t);

            void set_favorite(const bool f)
            {
                auto first_set{ m_Image.get_icon_name().empty() };
                if (f)
                    m_Image.set_from_icon_name("starred-symbolic", Gtk::ICON_SIZE_BUTTON);
                else
                    m_Image.set_from_icon_name("non-starred-symbolic", Gtk::ICON_SIZE_BUTTON);

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
        void on_toggle_show_headers(bool state);

        SignalNewTabTagType signal_new_tab_tag() const { return m_SignalNewTabTag; }

    protected:
        void on_realize() override;
        bool on_button_press_event(GdkEventButton* e) override;

    private:
        void on_entry_value_changed();
        bool on_tag_button_press_event(GdkEventButton* e, Row* row);
        void on_tag_favorite_toggled(Row* row);

        void header_func(Gtk::ListBoxRow* a, Gtk::ListBoxRow* b);

        Gtk::Menu* m_PopupMenu;
        Glib::RefPtr<Gio::SimpleAction> m_ShowTagTypeHeaders;

        TagEntry* m_TagEntry;
        Gtk::ScrolledWindow* m_ScrolledWindow;
        std::vector<Tag>*m_Tags, &m_FavoriteTags;

        SignalNewTabTagType m_SignalNewTabTag;
    };
}
