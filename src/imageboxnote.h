#pragma once

#include <gtkmm.h>

#include "note.h"

namespace AhoViewer
{
    class ImageBoxNote final : public Gtk::Widget
    {
    public:
        ImageBoxNote(const Note &note);
        virtual ~ImageBoxNote() = default;

        void set_scale(const double scale) { m_Scale = scale; }
        int get_x() const { return m_Note.x * m_Scale; }
        int get_y() const { return m_Note.y * m_Scale; }
    protected:
        Gtk::SizeRequestMode get_request_mode_vfunc() const override;
        void get_preferred_width_vfunc(int& minimum_width, int& natural_width) const override;
        void get_preferred_height_for_width_vfunc(int width, int& minimum_height, int& natural_height) const  override;
        void get_preferred_height_vfunc(int& minimum_height, int& natural_height) const override;
        void get_preferred_width_for_height_vfunc(int height, int& minimum_width, int& natural_width) const override;
        void on_size_allocate(Gtk::Allocation& allocation) override;
        void on_realize() override;
        void on_unrealize() override;
        bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;
        bool on_enter_notify_event(GdkEventCrossing *e) override;
        bool on_leave_notify_event(GdkEventCrossing *e) override;

        Glib::RefPtr<Gdk::Window> m_refGdkWindow;
        Glib::RefPtr<Gtk::CssProvider> m_refCssProvider;

        const Note &m_Note;
        Gtk::Popover m_Popover;

        double m_Scale{ 1. };
        bool m_Hovered{ false };
        sigc::connection m_TimeoutConn;
    };
}
