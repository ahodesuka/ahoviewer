#pragma once

#include "../util.h"

#include <gtkmm.h>

namespace AhoViewer::Booru
{
    class TagView;
    class InfoBox : public Gtk::EventBox
    {
    public:
        InfoBox(BaseObjectType* cobj, const Glib::RefPtr<Gtk::Builder>& bldr);
        ~InfoBox() override = default;

        void set_info(const PostInfo& post_info);
        void clear();

        void show();
        void hide();
        bool timeout_hide();
        bool is_visible() const { return m_IsVisible; }
        GdkWindow* get_source_label_window() const
        {
            return m_SourceLabel->get_window() ? m_SourceLabel->get_window()->gobj() : nullptr;
        }

        void on_toggle_auto_hide();

    protected:
        bool on_button_press_event(GdkEventButton* e) override;

    private:
        std::unique_ptr<Gtk::Menu> m_PopupMenu{ nullptr };

        bool m_InfoSet{ false }, m_IsVisible{ false };
        Gtk::Revealer* m_Revealer;
        Gtk::Label *m_DateLabel, *m_SourceLabel, *m_RatingLabel, *m_ScoreLabel;

        sigc::connection m_HideConn;
    };
}
