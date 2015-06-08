#ifndef _IMAGEBOX_H_
#define _IMAGEBOX_H_

#include <gtkmm.h>

#include "image.h"

namespace AhoViewer
{
    class StatusBar;
    class ImageBox : public Gtk::EventBox
    {
    public:
        enum class ZoomMode : char
        {
            AUTO_FIT   = 'A',
            FIT_WIDTH  = 'W',
            FIT_HEIGHT = 'H',
            MANUAL     = 'M',
        };

        ImageBox(BaseObjectType*, const Glib::RefPtr<Gtk::Builder>&);
        virtual ~ImageBox();

        void queue_draw_image(const bool scroll = false);
        void set_image(const std::shared_ptr<Image> &image);
        void clear_image();
        void update_background_color();

        bool is_slideshow_running();
        void reset_slideshow();
        void start_slideshow();
        void stop_slideshow();

        ZoomMode get_zoom_mode() const;
        void set_zoom_mode(const ZoomMode);

        void set_statusbar(StatusBar *sb);

        // Action callbacks {{{
        void on_zoom_in();
        void on_zoom_out();
        void on_reset_zoom();
        void on_toggle_scrollbars();
        void on_scroll_up();
        void on_scroll_down();
        void on_scroll_left();
        void on_scroll_right();
        // }}}
    protected:
        virtual void on_realize();
        virtual bool on_button_press_event(GdkEventButton*);
        virtual bool on_button_release_event(GdkEventButton*);
        virtual bool on_motion_notify_event(GdkEventMotion*);
        virtual bool on_scroll_event(GdkEventScroll*);
    private:
        void draw_image(const bool _scroll);
        bool update_animation();
        void scroll(const int x, const int y, const bool panning = false, const bool fromSlideshow = false);
        void smooth_scroll(const int, const Glib::RefPtr<Gtk::Adjustment>&);
        bool update_smooth_scroll();
        void zoom(const uint32_t percent);
        bool advance_slideshow();

        static const double SmoothScrollStep;

        Gtk::Layout *m_Layout;
        Gtk::HScrollbar *m_HScroll;
        Gtk::VScrollbar *m_VScroll;
        Gtk::Image *m_GtkImage;
        Gtk::Menu *m_PopupMenu;
        Glib::RefPtr<Gtk::Adjustment> m_HAdjust, m_VAdjust, m_ScrollAdjust;
        Glib::RefPtr<Gtk::UIManager> m_UIManager;
        Glib::RefPtr<Gtk::Action> m_NextAction, m_PreviousAction;

        StatusBar *m_StatusBar;

        const Gdk::Cursor m_LeftPtrCursor, m_FleurCursor;

        std::shared_ptr<Image> m_Image;
        Glib::RefPtr<Gdk::PixbufAnimation> m_PixbufAnim;
        Glib::RefPtr<Gdk::PixbufAnimationIter> m_PixbufAnimIter;
        sigc::connection m_DrawConn, m_ImageConn, m_ScrollConn, m_SlideshowConn, m_AnimConn;

        bool m_Scroll, m_RedrawQueued;
        ZoomMode m_ZoomMode;
        uint32_t m_ZoomPercent;
        double m_PressX, m_PreviousX,
               m_PressY, m_PreviousY,
               m_ScrollTime, m_ScrollDuration,
               m_ScrollStart, m_ScrollTarget;
    };
}


#endif /* _IMAGEBOX_H_ */
