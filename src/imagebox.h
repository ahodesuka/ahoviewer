#pragma once

#include "config.h"
#include "image.h"
#include "util.h"

#include <gtkmm.h>

#ifdef HAVE_GSTREAMER
#include <gst/gst.h>
#endif // HAVE_GSTREAMER

namespace AhoViewer
{
    class ImageBoxNote;
    class MainWindow;
    class StatusBar;
    class ImageBox : public Gtk::ScrolledWindow
    {
    public:
        ImageBox(BaseObjectType*, const Glib::RefPtr<Gtk::Builder>&);
        ~ImageBox() override;

        void queue_draw_image(const bool scroll = false);
        void set_image(const std::shared_ptr<Image>& image);
        void clear_image();
        void update_background_color();
        void cursor_timeout();

        bool is_slideshow_running() { return !!m_SlideshowConn; }
        void reset_slideshow();
        void toggle_slideshow();

        int get_orig_width() const { return m_OrigWidth; }
        int get_orig_height() const { return m_OrigHeight; }

        double get_scale() const { return m_Scale; }

        ZoomMode get_zoom_mode() const { return m_ZoomMode; }
        void set_zoom_mode(const ZoomMode);

        ScrollPos get_scroll_position() const
        {
            return { get_hadjustment()->get_value(), get_vadjustment()->get_value(), m_ZoomMode };
        }
        // Scrollbar positions to be restored when next image is drawn
        void set_restore_scroll_position(const ScrollPos& s) { m_RestoreScrollPos = s; }

        sigc::signal<void> signal_slideshow_ended() const { return m_SignalSlideshowEnded; }
        sigc::signal<void> signal_image_drawn() const { return m_SignalImageDrawn; }

        static Gdk::RGBA DefaultBGColor;

        // Action callbacks {{{
        void on_zoom_in();
        void on_zoom_out();
        void on_reset_zoom();
        void on_scroll_up();
        void on_scroll_down();
        void on_scroll_left();
        void on_scroll_right();
        // }}}
    protected:
        void on_realize() override;
        bool on_button_press_event(GdkEventButton* e) override;
        bool on_button_release_event(GdkEventButton* e) override;
        bool on_motion_notify_event(GdkEventMotion* e) override;
        bool on_scroll_event(GdkEventScroll* e) override;

    private:
        struct SmoothScroll
        {
            SmoothScroll(Glib::RefPtr<Gtk::Adjustment> adj) : adj(adj) { }

            double start_time, end_time, start{ 0 }, end{ 0 };
            guint64 id{ 0 };
            bool scrolling{ false };
            Glib::RefPtr<Gtk::Adjustment> adj;
        };
        void get_scale_and_position(int& w, int& h, int& x, int& y);
        void draw_image(bool scroll);
        bool update_animation();
        void scroll(const int x,
                    const int y,
                    const bool panning        = false,
                    const bool from_slideshow = false);
        void smooth_scroll(const int amount, ImageBox::SmoothScroll& ss);
        void remove_scroll_callback(ImageBox::SmoothScroll& ss);
        void zoom(const uint32_t percent);

        bool advance_slideshow();
        bool on_cursor_timeout();
        void on_notes_changed();
        void clear_notes();
        void update_notes();

        Gtk::Fixed *m_Fixed, *m_NoteFixed;
        Gtk::Overlay* m_Overlay;
        Gtk::Image* m_GtkImage;
        Gtk::Widget* m_GstWidget;
        Gtk::Menu* m_PopupMenu;
        Glib::RefPtr<Gtk::UIManager> m_UIManager;
        Glib::RefPtr<Gtk::Action> m_NextAction, m_PreviousAction;
        SmoothScroll m_HSmoothScroll, m_VSmoothScroll;

#ifdef HAVE_GSTREAMER
        static GstBusSyncReply create_window(GstBus* bus, GstMessage* msg, void* userp);
        static gboolean bus_cb(GstBus*, GstMessage* msg, void* userp);

        void on_set_volume();
        void reset_gstreamer_pipeline();
        GstElement* create_video_sink(const std::string& name);

        GstElement *m_Playbin{ nullptr }, *m_VideoSink{ nullptr };
        guintptr m_WindowHandle{ 0 };
        bool m_Playing{ false }, m_UsingWayland{ false };
#endif // HAVE_GSTREAMER

        StatusBar* m_StatusBar;
        const MainWindow* m_MainWindow;

        const Glib::RefPtr<Gdk::Cursor> m_LeftPtrCursor, m_FleurCursor, m_BlankCursor;

        int m_OrigWidth{ 0 }, m_OrigHeight{ 0 };

        std::shared_ptr<Image> m_Image;
        sigc::connection m_AnimConn, m_CursorConn, m_DrawConn, m_ImageConn, m_NotesConn,
            m_SlideshowConn, m_StyleUpdatedConn;

        bool m_FirstDraw{ false }, m_RedrawQueued{ false }, m_Loading{ false },
            m_ZoomScroll{ false };
        ZoomMode m_ZoomMode;
        ScrollPos m_RestoreScrollPos;
        // TODO: add setting for this
        uint32_t m_ZoomPercent{ 100 };
        double m_Scale{ 0 }, m_PressX, m_PreviousX, m_PressY, m_PreviousY;

        std::vector<ImageBoxNote*> m_Notes;

        sigc::signal<void> m_SignalSlideshowEnded, m_SignalImageDrawn;
    };
}
