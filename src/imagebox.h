#ifndef _IMAGEBOX_H_
#define _IMAGEBOX_H_

#include <epoxy/gl.h>
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
        class GLArea : public Gtk::GLArea
        {
        public:
            GLArea(BaseObjectType*, const Glib::RefPtr<Gtk::Builder>&);
            virtual ~GLArea() = default;
            void clear();

            void set_image(const Glib::RefPtr<Gdk::Pixbuf> &pixbuf);
            void set_filtering(const bool f) { m_Filtering = f; }
        protected:
            virtual void on_realize() override;
            virtual void on_unrealize() override;
            virtual bool on_render(const Glib::RefPtr<Gdk::GLContext>&) override;
            virtual void on_resize(int w, int h) override;
        private:
            GLuint compile_shader(GLenum type, const GLchar *source);

            bool m_Filtering {false};
            GLuint m_Texture { 0 },
                   m_Filter  { 0 },
                   m_Vao     { 0 },
                   m_Mvp     { 0 },
                   m_VBuffer { 0 },
                   m_TBuffer { 0 },
                   m_Program { 0 },
                   m_Model   { 0 },
                   m_View    { 0 },
                   m_Proj    { 0 };
        };
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
        void on_pixbuf_changed();

        static constexpr double SmoothScrollStep = 1000.0 / 60.0;

        Gtk::Layout *m_Layout;
        Gtk::Image *m_GtkImage;
        Gtk::Widget *m_GstWidget;
        ImageBox::GLArea *m_GLArea;
        Gtk::Menu *m_PopupMenu;
        Glib::RefPtr<Gtk::Adjustment> m_ScrollAdjust;
        Glib::RefPtr<Gtk::UIManager> m_UIManager;
        Glib::RefPtr<Gtk::Action> m_NextAction, m_PreviousAction;

#ifdef HAVE_GSTREAMER
        static gboolean bus_cb(GstBus*, GstMessage *message, void *userp);

        GstElement *m_Playbin { nullptr };
        bool m_Playing { false };
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
                         m_StyleUpdatedConn;

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

    const bool use_opengl { true };
}


#endif /* _IMAGEBOX_H_ */
