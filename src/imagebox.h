#ifndef _IMAGEBOX_H_
#define _IMAGEBOX_H_

#include <atomic>
#include <gtkmm.h>

#include "config.h"
#include "image.h"

#ifdef HAVE_GSTREAMER
#include <gst/gst.h>
#endif // HAVE_GSTREAMER

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
        virtual ~ImageBox() = default;

        void queue_draw_image(const bool scroll = false);
        void set_image(const std::shared_ptr<Image> &image);
        void clear_image();
        void update_background_color();
        void cursor_timeout();

        bool is_slideshow_running() { return !!m_SlideshowConn; }
        void reset_slideshow();
        void toggle_slideshow();

        ZoomMode get_zoom_mode() const;
        void set_zoom_mode(const ZoomMode);

        void set_statusbar(StatusBar *sb);

        sigc::signal<void> signal_slideshow_ended() const { return m_SlideshowEndedSignal; }

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
        void draw_image(bool scroll);
        bool get_scaled_size(int origWidth, int origHeight, int &w, int &h);
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
        Gtk::DrawingArea *m_DrawingArea;
        Gtk::Menu *m_PopupMenu;
        Glib::RefPtr<Gtk::Adjustment> m_HAdjust, m_VAdjust, m_ScrollAdjust;
        Glib::RefPtr<Gtk::UIManager> m_UIManager;
        Glib::RefPtr<Gtk::Action> m_NextAction, m_PreviousAction;

#ifdef HAVE_GSTREAMER
        static GstBusSyncReply create_window(GstBus*, GstMessage *message, void *userp);
        static gboolean bus_cb(GstBus*, GstMessage *message, void *userp);

        GstElement *m_Playbin,
                   *m_VideoSink;
        guintptr m_WindowHandle;
        bool m_Playing = false;
#endif // HAVE_GSTREAMER

        StatusBar *m_StatusBar;

        Gdk::Color m_BGColor;
        const Gdk::Cursor m_LeftPtrCursor, m_FleurCursor, m_BlankCursor;

        std::shared_ptr<Image> m_Image;
        Glib::RefPtr<Gdk::PixbufAnimation> m_PixbufAnim;
        Glib::RefPtr<Gdk::PixbufAnimationIter> m_PixbufAnimIter;
        sigc::connection m_AnimConn,
                         m_CursorConn,
                         m_DrawConn,
                         m_ImageConn,
                         m_ScrollConn,
                         m_SlideshowConn;

        std::atomic<int> m_WindowWidth,
                         m_WindowHeight,
                         m_LayoutWidth,
                         m_LayoutHeight;
        bool m_FirstDraw, m_RedrawQueued, m_Loading, m_HideScrollbars, m_ZoomScroll;
        ZoomMode m_ZoomMode;
        uint32_t m_ZoomPercent;
        double m_PressX, m_PreviousX,
               m_PressY, m_PreviousY,
               m_ScrollTime, m_ScrollDuration,
               m_ScrollStart, m_ScrollTarget;

        sigc::signal<void> m_SlideshowEndedSignal;
    };
}


#endif /* _IMAGEBOX_H_ */
