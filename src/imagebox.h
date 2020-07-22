#ifndef _IMAGEBOX_H_
#define _IMAGEBOX_H_

#include <gtkmm.h>

#include "config.h"
#include "image.h"
#include "util.h"

#ifdef HAVE_GSTREAMER
#include <gst/gst.h>
#endif // HAVE_GSTREAMER

namespace AhoViewer
{
    class MainWindow;
    class StatusBar;
    class ImageBox : public Gtk::ScrolledWindow
    {
    public:
        ImageBox(BaseObjectType*, const Glib::RefPtr<Gtk::Builder>&);
        virtual ~ImageBox() override;

        void queue_draw_image(const bool scroll = false);
        void set_image(const std::shared_ptr<Image> &image);
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
        void set_restore_scroll_position(const ScrollPos &s) { m_RestoreScrollPos = s; }

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
        virtual void on_realize() override;
        virtual bool on_button_press_event(GdkEventButton *e) override;
        virtual bool on_button_release_event(GdkEventButton *e) override;
        virtual bool on_motion_notify_event(GdkEventMotion *e) override;
        virtual bool on_scroll_event(GdkEventScroll *e) override;
    private:
        void get_scale_and_position(int &w, int &h, int &x, int &y);
        void draw_image(bool scroll);
        bool update_animation();
        void scroll(const int x, const int y, const bool panning = false, const bool fromSlideshow = false);
        void smooth_scroll(const int, const Glib::RefPtr<Gtk::Adjustment>&);
        bool update_smooth_scroll();
        void zoom(const uint32_t percent);

        bool advance_slideshow();
        bool on_cursor_timeout();

        static constexpr double SmoothScrollStep = 1000.0 / 60.0;

        Gtk::Layout *m_Layout;
        Gtk::Image *m_GtkImage;
        Gtk::DrawingArea *m_DrawingArea;
        Gtk::Menu *m_PopupMenu;
        Glib::RefPtr<Gtk::Adjustment> m_ScrollAdjust;
        Glib::RefPtr<Gtk::UIManager> m_UIManager;
        Glib::RefPtr<Gtk::Action> m_NextAction, m_PreviousAction;

#ifdef HAVE_GSTREAMER
        static GstBusSyncReply create_window(GstBus *bus, GstMessage *msg, void *userp);
        static gboolean bus_cb(GstBus*, GstMessage *msg, void *userp);
        static GstPadProbeReturn buffer_probe_cb(GstPad*, GstPadProbeInfo*, void *userp);
        static void on_about_to_finish(GstElement*, void *userp);
        static void on_source_setup(GstElement*, GstElement *source, void *userp);
        static void on_need_data(GstElement*, guint, void *userp);
        static void on_enough_data(GstElement*, void *userp);
        static void on_video_changed(GstElement*, void *userp);

        void create_playbin();
        void reset_gstreamer_pipeline();
        GstElement* create_video_sink(const std::string &name);

        void on_streams_selected(GstMessage *msg);
        void on_curler_write(const unsigned char *d, size_t l);
        void on_curler_finished();
        void on_set_volume();
        void on_application_message(GstMessage *msg);

        GstElement *m_Playbin   { nullptr },
                   *m_VideoSink { nullptr },
                   *m_AppSrc    { nullptr };

        guintptr m_WindowHandle { 0 };
        gulong m_PadProbeID    { 0 },
               m_SourceSetupID { 0 },
               m_NeedDataID    { 0 },
               m_EnoughDataID  { 0 };
        GstPad *m_VideoPad { nullptr };

        bool m_Playing          { false },
             // Used to tell the draw_image method that we were waiting for
             // enough data to know if the video is playable
             m_Prerolling       { false },
             // Whether we are waiting for the streams to be selected during
             // initial playback startup
             m_WaitingForStream { false },
             // Keeps track of whether we have sent the appsrc any data yet
             m_FirstFill        { true };
#endif // HAVE_GSTREAMER

        StatusBar *m_StatusBar;
        const MainWindow *m_MainWindow;

        const Glib::RefPtr<Gdk::Cursor> m_LeftPtrCursor, m_FleurCursor, m_BlankCursor;

        int m_OrigWidth  { 0 },
            m_OrigHeight { 0 };

        std::shared_ptr<Image> m_Image;
        sigc::connection m_AnimConn,
                         m_CursorConn,
                         m_DrawConn,
                         m_ImageConn,
                         m_ScrollConn,
                         m_SlideshowConn,
                         m_StyleUpdatedConn,
                         m_CurlerWriteConn,
                         m_CurlerFinishedConn;

        bool m_FirstDraw    { false },
             m_RedrawQueued { false },
             m_Loading      { false },
             m_ZoomScroll   { false };
        ZoomMode m_ZoomMode;
        ScrollPos m_RestoreScrollPos;
        // TODO: add setting for this
        uint32_t m_ZoomPercent { 100 };
        double m_Scale { 0 },
               m_PressX, m_PreviousX,
               m_PressY, m_PreviousY,
               m_ScrollTime, m_ScrollDuration,
               m_ScrollStart, m_ScrollTarget;

        sigc::signal<void> m_SignalSlideshowEnded,
                           m_SignalImageDrawn;
    };
}


#endif /* _IMAGEBOX_H_ */
