#include "imagebox.h"
using namespace AhoViewer;

#include "imageboxnote.h"
#include "mainwindow.h"
#include "settings.h"
#include "statusbar.h"

#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif // ! M_PI

#ifdef HAVE_GSTREAMER
#include <gst/audio/streamvolume.h>
#include <gst/video/videooverlay.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif // GDK_WINDOWING_X11
#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif // GDK_WINDOWING_WAYLAND
#ifdef GDK_WINDOWING_WIN32
#include <gdk/gdkwin32.h>
#endif // GDK_WINDOWING_WIN32
#ifdef GDK_WINDOWING_QUARTZ
#include <gdk/gdkquartz.h>
#endif // GDK_WINDOWING_QUARTZ

#ifdef GDK_WINDOWING_WAYLAND
#define GST_WAYLAND_DISPLAY_HANDLE_CONTEXT_TYPE "GstWaylandDisplayHandleContextType"

bool gst_is_wayland_display_handle_need_context_message(GstMessage* msg)
{
    const gchar* type = nullptr;

    if (!GST_IS_MESSAGE(msg))
        return false;

    if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_NEED_CONTEXT &&
        gst_message_parse_context_type(msg, &type))
        return !g_strcmp0(type, GST_WAYLAND_DISPLAY_HANDLE_CONTEXT_TYPE);

    return false;
}

GstContext* gst_wayland_display_handle_context_new(struct wl_display* display)
{
    GstContext* context = gst_context_new(GST_WAYLAND_DISPLAY_HANDLE_CONTEXT_TYPE, TRUE);
    gst_structure_set(
        gst_context_writable_structure(context), "handle", G_TYPE_POINTER, display, NULL);
    return context;
}
#endif // GDK_WINDOWING_WAYLAND

GstBusSyncReply ImageBox::create_window(GstBus* bus, GstMessage* msg, void* userp)
{
#ifdef GDK_WINDOWING_WAYLAND
    GdkDisplayManager* dpm{ gdk_display_manager_get() };
    GdkDisplay* dpy{ gdk_display_manager_get_default_display(dpm) };

    if (GDK_IS_WAYLAND_DISPLAY(dpy) && gst_is_wayland_display_handle_need_context_message(msg))
    {
        auto* self                    = static_cast<ImageBox*>(userp);
        GdkDisplay* display           = self->m_GstWidget->get_display()->gobj();
        struct wl_display* dpy_handle = gdk_wayland_display_get_wl_display(display);
        GstContext* ctx               = gst_wayland_display_handle_context_new(dpy_handle);

        gst_element_set_context(GST_ELEMENT(GST_MESSAGE_SRC(msg)), ctx);

        gst_message_unref(msg);
        return GST_BUS_DROP;
    }
    else
#endif // GDK_WINDOWING_WAYLAND
        if (gst_is_video_overlay_prepare_window_handle_message(msg))
    {
        auto* self = static_cast<ImageBox*>(userp);

        gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(self->m_VideoSink),
                                            self->m_WindowHandle);

        // XXX: This is needed for waylandsink otherwise the initial draw will
        // only be black pixels
        if (self->m_UsingWayland)
            gst_video_overlay_set_render_rectangle(
                GST_VIDEO_OVERLAY(self->m_VideoSink), 0, 0, 10, 10);

        gst_message_unref(msg);
        return GST_BUS_DROP;
    }

    return GST_BUS_PASS;
}

gboolean ImageBox::bus_cb(GstBus*, GstMessage* msg, void* userp)
{
    auto* self = static_cast<ImageBox*>(userp);

    switch (GST_MESSAGE_TYPE(msg))
    {
    case GST_MESSAGE_EOS:
        gst_element_seek_simple(self->m_Playbin, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, 0);
        break;
    case GST_MESSAGE_ERROR:
    {
        GError* err{ nullptr };
        gchar* dbg_info{ nullptr };

        gst_message_parse_error(msg, &err, &dbg_info);
        std::cerr << "ERROR from element " << GST_OBJECT_NAME(msg->src) << ": " << err->message
                  << std::endl;
        std::cerr << "Debugging info: " << (dbg_info ? dbg_info : "none") << std::endl;
        g_error_free(err);
        g_free(dbg_info);
    }
    break;
    default:
        break;
    }

    return TRUE;
}

// Set volume based on Volume setting
void ImageBox::on_set_volume()
{
    // TODO: add a range widget that controls this value
    gst_stream_volume_set_volume(
        GST_STREAM_VOLUME(m_Playbin),
        GST_STREAM_VOLUME_FORMAT_CUBIC,
        std::min(static_cast<double>(Settings.get_int("Volume")) / 100, 1.0));
}

void ImageBox::reset_gstreamer_pipeline()
{
    gst_element_set_state(m_Playbin, GST_STATE_NULL);
    m_Playing = false;
}

// Attempts to create a GstElement of type @name, and prints a message if it fails
GstElement* ImageBox::create_video_sink(const std::string& name)
{
    GstElement* sink = gst_element_factory_make(name.c_str(), name.c_str());
    if (!sink)
        std::cerr << "Failed to create videosink of type '" << name << "'" << std::endl;

    return sink;
}
#endif // HAVE_GSTREAMER

Gdk::RGBA ImageBox::DefaultBGColor = Gdk::RGBA();

ImageBox::ImageBox(BaseObjectType* cobj, const Glib::RefPtr<Gtk::Builder>& bldr)
    : Gtk::ScrolledWindow{ cobj },
      m_HSmoothScroll{ get_hadjustment() },
      m_VSmoothScroll{ get_vadjustment() },
      m_LeftPtrCursor{ Gdk::Cursor::create(Gdk::LEFT_PTR) },
      m_FleurCursor{ Gdk::Cursor::create(Gdk::FLEUR) },
      m_BlankCursor{ Gdk::Cursor::create(Gdk::BLANK_CURSOR) },
      m_ZoomMode{ Settings.get_zoom_mode() },
      m_RestoreScrollPos{ -1, -1, m_ZoomMode }
{
    bldr->get_widget("ImageBox::Fixed", m_Fixed);
    bldr->get_widget("ImageBox::Overlay", m_Overlay);
    bldr->get_widget("ImageBox::Image", m_GtkImage);
    bldr->get_widget("ImageBox::NoteFixed", m_NoteFixed);
    bldr->get_widget("ImageBox::DrawingArea", m_GstWidget);
    bldr->get_widget_derived("StatusBar", m_StatusBar);
    bldr->get_widget_derived("MainWindow", m_MainWindow);

    m_UIManager = Glib::RefPtr<Gtk::UIManager>::cast_static(bldr->get_object("UIManager"));

#ifdef HAVE_GSTREAMER
    // Prefer hardware accelerated decoders if they exist
    const auto hw_decoders = {
        "nvh264dec",       "nvh264sldec",    "nvh265dec",   "nvh265dec",   "nvmpeg2videodec",
        "nvmpeg4videodec", "nvmpegvideodec", "nvvp8dec",    "nvvp9dec",    "vaapih264dec",
        "vaapih264dec",    "vaapimpeg2dec",  "vaapivc1dec",
#ifdef _WIN32
        "d3d11h264dec",    "d3d11h265dec",   "d3d11vp8dec", "d3d11vp9dec",
#endif
    };
    GstRegistry* plugins_register{ gst_registry_get() };

    for (const auto& decoder : hw_decoders)
    {
        GstPluginFeature* feature{ gst_registry_lookup_feature(plugins_register, decoder) };
        if (feature)
        {
            gst_plugin_feature_set_rank(feature, GST_RANK_PRIMARY + 1);
            gst_object_unref(feature);
        }
    }

    auto videosink = Settings.get_string("VideoSink");

    if (!videosink.empty())
    {
        std::transform(videosink.begin(), videosink.end(), videosink.begin(), [](unsigned char c) {
            return std::tolower(c);
        });

        m_VideoSink = create_video_sink(videosink.c_str());
        if (!m_VideoSink)
            std::cerr << "Invalid VideoSink setting provided '" << videosink << "'" << std::endl;
        else if (videosink == "waylandsink")
            m_UsingWayland = true;
    }

    GdkDisplayManager* dpm{ gdk_display_manager_get() };
    [[maybe_unused]] GdkDisplay* dpy{ gdk_display_manager_get_default_display(dpm) };
    bool gtk_sink{ false };

#if defined(GDK_WINDOWING_X11) || defined(GDK_WINDOWING_WAYLAND)
    // gtkglsink is the preferred sink to use on linux
    if ((
#ifdef GDK_WINDOWING_X11
            GDK_IS_X11_DISPLAY(dpy)
#else
            false
#endif
            ||
#ifdef GDK_WINDOWING_WAYLAND
            GDK_IS_WAYLAND_DISPLAY(dpy)
#else
            false
#endif
                ) &&
        !m_VideoSink)
    {
        GstElement* gtkglsink{ gst_element_factory_make("gtkglsink", "gtksink") };
        if (gtkglsink)
        {
            GtkWidget* w{ nullptr };
            g_object_get(gtkglsink, "widget", &w, nullptr);

            if (w)
            {
                if ((m_VideoSink = gst_element_factory_make("glsinkbin", "glsinkbin")))
                {
                    m_Fixed->remove(*m_GstWidget);
                    delete m_GstWidget;

                    m_GstWidget = Glib::wrap(w);
                    m_Fixed->add(*m_GstWidget);
                    g_object_unref(w);

                    g_object_set(m_VideoSink, "sink", gtkglsink, nullptr);
                    gtk_sink = true;
                }
            }
        }
    }
#endif

#ifdef GDK_WINDOWING_X11
    // These sinks don't work with rgba visual set
    if (GDK_IS_X11_DISPLAY(dpy) && !m_VideoSink && !m_MainWindow->has_rgba_visual())
    {
        m_VideoSink = create_video_sink("xvimagesink");
        // Make sure the X server has the X video extension enabled
        if (m_VideoSink)
        {
            bool have_xvideo{ false };
            int nextens;
            char** extensions = XListExtensions(
                gdk_x11_display_get_xdisplay(Gdk::Display::get_default()->gobj()), &nextens);

            for (int i = 0; extensions != nullptr && i < nextens; ++i)
            {
                if (strcmp(extensions[i], "XVideo") == 0)
                {
                    have_xvideo = true;
                    break;
                }
            }

            if (extensions)
                XFreeExtensionList(extensions);

            if (!have_xvideo)
            {
                gst_object_unref(GST_OBJECT(m_VideoSink));
                m_VideoSink = nullptr;
                std::cerr << "xvimagesink created, but X video extension not supported by X server!"
                          << std::endl;
            }
        }

        if (!m_VideoSink)
            m_VideoSink = create_video_sink("ximagesink");
    }
#endif // GDK_WINDOWING_X11

#ifdef GDK_WINDOWING_WAYLAND
    if (GDK_IS_WAYLAND_DISPLAY(dpy) && !m_VideoSink)
    {
        m_VideoSink = create_video_sink("waylandsink");

        if (m_VideoSink)
            m_UsingWayland = true;
    }
#endif // GDK_WINDOWING_WAYLAND

#ifdef GDK_WINDOWING_WIN32
    if (GDK_IS_WIN32_DISPLAY(dpy) && !m_VideoSink)
        m_VideoSink = create_video_sink("d3dvideosink");
#endif // GDK_WINDOWING_WIN32

    // glimagesink should work on all platforms
    if (!m_VideoSink)
        m_VideoSink = create_video_sink("glimagesink");

    if (m_VideoSink)
    {
        char* name = gst_element_get_name(m_VideoSink);
        std::cout << "Using videosink of type '" << name << "'" << std::endl;
        // Prevent the glimagesink from handling events so the right click menu
        // can be shown when clicking on the drawing area
        if (g_strcmp0(name, "glimagesink") == 0)
            g_object_set(m_VideoSink, "handle-events", false, NULL);

        if (name)
            g_free(name);

        // Store the window handle in order to use it in ::create_window
        // Gtk sink does this for us automatically
        if (!gtk_sink)
        {
            m_GstWidget->signal_realize().connect([&, dpy]() {

#ifdef GDK_WINDOWING_X11
                if (GDK_IS_X11_DISPLAY(dpy))
                {
                    m_WindowHandle = GDK_WINDOW_XID(m_GstWidget->get_window()->gobj());
                }
                else
#endif // GDK_WINDOWING_X11
#ifdef GDK_WINDOWING_WAYLAND
                    if (GDK_IS_WAYLAND_DISPLAY(dpy))
                {
                    m_WindowHandle = (guintptr)gdk_wayland_window_get_wl_surface(
                        m_GstWidget->get_window()->gobj());
                }
                else
#endif // GDK_WINDOWING_WAYLAND
#ifdef GDK_WINDOWING_WIN32
                    if (GDK_IS_WIN32_DISPLAY(dpy))
                {
                    m_WindowHandle = (guintptr)GDK_WINDOW_HWND(m_GstWidget->get_window()->gobj());
                }
                else
#endif // GDK_WINDOWING_WIN32
#ifdef GDK_WINDOWING_QUARTZ
                    if (GDK_IS_QUARTZ_DISPLAY(dpy))
                {
                    m_WindowHandle =
                        (guintptr)gdk_quartz_window_get_nsview(m_GstWidget->get_window()->gobj());
                }
                else
#endif // GDK_WINDOWING_QUARTZ
                {
                    std::cerr << "Unsupported Gtk backend" << std::endl;
                    return;
                }

                gst_element_set_state(m_Playbin, GST_STATE_READY);

                // This isn't needed right away and causes an input focus issue on Windows
                Glib::signal_idle().connect_once([&]() {
                    if (!m_Playing)
                        m_GstWidget->hide();
                });
            });
        }

        gst_object_ref(m_VideoSink);
    }

    // playbin3 will break things.
    Glib::setenv("USE_PLAYBIN3", "0", true);
    m_Playbin = gst_element_factory_make("playbin", "playbin");
    g_object_set(m_Playbin,
                 // For now users can put
                 // AudioSink = "autoaudiosink";
                 // into the config file and have sound if they have the correct gstreamer plugins
                 // Can also set Volume = 0-100; in config to control it
                 "audio-sink",
                 gst_element_factory_make(Settings.get_string("AudioSink").c_str(), "audiosink"),
                 // If sink is null it will fallback to autovideosink
                 "video-sink",
                 m_VideoSink,
                 // draw_image takes care of it
                 "force-aspect-ratio",
                 false,
                 NULL);

    GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(m_Playbin));
    gst_bus_set_sync_handler(bus, &ImageBox::create_window, this, nullptr);
    gst_bus_add_watch(bus, &ImageBox::bus_cb, this);
    g_object_unref(bus);
#endif // HAVE_GSTREAMER

    m_StyleUpdatedConn = m_Fixed->signal_style_updated().connect(
        [&]() { m_Fixed->get_style_context()->lookup_color("theme_bg_color", DefaultBGColor); });
}

ImageBox::~ImageBox()
{
    clear_image();
#ifdef HAVE_GSTREAMER
    gst_object_unref(GST_OBJECT(m_Playbin));
#endif // HAVE_GSTREAMER
}

void ImageBox::queue_draw_image(const bool scroll)
{
    if (!get_realized() || !m_Image || (m_RedrawQueued && !(scroll || m_Loading)))
        return;

    m_DrawConn.disconnect();
    m_RedrawQueued = true;
    m_DrawConn     = Glib::signal_idle().connect(
        sigc::bind_return(sigc::bind(sigc::mem_fun(*this, &ImageBox::draw_image), scroll), false),
        Glib::PRIORITY_HIGH_IDLE);
}

void ImageBox::set_image(const std::shared_ptr<Image>& image)
{
    // very unlikely that image would be null here
    if (!image)
        return;

    if (image != m_Image)
    {
        m_AnimConn.disconnect();
        m_ImageConn.disconnect();
        m_NotesConn.disconnect();
        remove_scroll_callback(m_HSmoothScroll);
        remove_scroll_callback(m_VSmoothScroll);
        reset_slideshow();
        clear_notes();

#ifdef HAVE_GSTREAMER
        reset_gstreamer_pipeline();
        m_GstWidget->hide();
#endif // HAVE_GSTREAMER

        // reset GIF stuff so if it stays cached and is viewed again it will
        // start from the first frame
        if (m_Image)
            m_Image->reset_gif_animation();

        m_Image     = image;
        m_FirstDraw = m_Loading = true;

        m_ImageConn = m_Image->signal_pixbuf_changed().connect(
            sigc::bind(sigc::mem_fun(*this, &ImageBox::queue_draw_image), false));

        // Maybe the notes havent been downloaded yet
        if (m_Image->get_notes().empty())
            m_NotesConn = m_Image->signal_notes_changed().connect(
                sigc::mem_fun(*this, &ImageBox::on_notes_changed));

        queue_draw_image(true);
    }
    else
    {
        queue_draw_image();
    }
}

void ImageBox::clear_image()
{
    m_SlideshowConn.disconnect();
    m_ImageConn.disconnect();
    m_NotesConn.disconnect();
    m_DrawConn.disconnect();
    m_AnimConn.disconnect();
    m_GtkImage->clear();
    m_Overlay->hide();
    m_GstWidget->hide();
    m_Fixed->set_size_request(0, 0);

    remove_scroll_callback(m_HSmoothScroll);
    remove_scroll_callback(m_VSmoothScroll);
    clear_notes();

#ifdef HAVE_GSTREAMER
    reset_gstreamer_pipeline();
#endif // HAVE_GSTREAMER

    m_StatusBar->clear_resolution();
    m_Image = nullptr;
}

void ImageBox::update_background_color()
{
    auto css = Gtk::CssProvider::create();
    css->load_from_data(Glib::ustring::compose("scrolledwindow#ImageBox{background:%1;}",
                                               Settings.get_background_color().to_string()));
    get_style_context()->add_provider(css, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

void ImageBox::cursor_timeout()
{
    m_CursorConn.disconnect();
    m_Fixed->get_window()->set_cursor(m_LeftPtrCursor);

    if (Settings.get_int("CursorHideDelay") <= 0)
        return;

    m_CursorConn = Glib::signal_timeout().connect_seconds(
        sigc::mem_fun(*this, &ImageBox::on_cursor_timeout), Settings.get_int("CursorHideDelay"));
}

void ImageBox::reset_slideshow()
{
    if (m_SlideshowConn)
    {
        m_SlideshowConn.disconnect();
        toggle_slideshow();
    }
}

void ImageBox::toggle_slideshow()
{
    if (!m_SlideshowConn)
    {
        m_SlideshowConn = Glib::signal_timeout().connect_seconds(
            sigc::mem_fun(*this, &ImageBox::advance_slideshow), Settings.get_int("SlideshowDelay"));
    }
    else
    {
        m_SlideshowConn.disconnect();
    }
}

void ImageBox::set_zoom_mode(const ZoomMode mode)
{
    if (mode != m_ZoomMode)
    {
        Settings.set_zoom_mode(mode);
        m_ZoomMode = mode;
        queue_draw_image(true);
    }
}

void ImageBox::on_zoom_in()
{
    zoom(m_ZoomPercent + 10);
}

void ImageBox::on_zoom_out()
{
    zoom(m_ZoomPercent - 10);
}

void ImageBox::on_reset_zoom()
{
    zoom(100);
}

void ImageBox::on_scroll_up()
{
    scroll(0, -300);
}

void ImageBox::on_scroll_down()
{
    scroll(0, 300);
}

void ImageBox::on_scroll_left()
{
    scroll(-300, 0);
}

void ImageBox::on_scroll_right()
{
    scroll(300, 0);
}

void ImageBox::on_realize()
{
    m_PopupMenu = static_cast<Gtk::Menu*>(m_UIManager->get_widget("/PopupMenu"));

    Glib::RefPtr<Gtk::ActionGroup> action_group =
        static_cast<std::vector<Glib::RefPtr<Gtk::ActionGroup>>>(
            m_UIManager->get_action_groups())[0];

    m_NextAction     = action_group->get_action("NextImage");
    m_PreviousAction = action_group->get_action("PreviousImage");

    m_Fixed->get_style_context()->lookup_color("theme_bg_color", DefaultBGColor);
    update_background_color();

    Gtk::ScrolledWindow::on_realize();
}

bool ImageBox::on_button_press_event(GdkEventButton* e)
{
    grab_focus();
    cursor_timeout();

    // Ignore double/triple clicks
    if (e->type == GDK_BUTTON_PRESS)
    {
        switch (e->button)
        {
        case 1:
        case 2:
            m_PressX = m_PreviousX = e->x_root;
            m_PressY = m_PreviousY = e->y_root;
            return true;
        case 3:
            m_PopupMenu->popup_at_pointer(reinterpret_cast<GdkEvent*>(e));
            m_CursorConn.disconnect();
            return true;
        case 8: // Back
            m_PreviousAction->activate();
            return true;
        case 9: // Forward
            m_NextAction->activate();
            return true;
        }
    }

    return Gtk::ScrolledWindow::on_button_press_event(e);
}

bool ImageBox::on_button_release_event(GdkEventButton* e)
{
    if (e->button == 1 || e->button == 2)
    {
        m_Fixed->get_window()->set_cursor(m_LeftPtrCursor);

        if (e->button == 1 && m_PressX == m_PreviousX && m_PressY == m_PreviousY)
        {
            if (Settings.get_bool("SmartNavigation"))
            {
                int w, h, x, y, ww, wh;
                m_MainWindow->get_drawable_area_size(w, h);
                m_MainWindow->get_size(ww, wh);
                m_MainWindow->get_position(x, y);
                x += ww - w;

                if (e->x_root - x < w / 2)
                {
                    m_PreviousAction->activate();
                    return true;
                }
            }

            m_NextAction->activate();
        }

        return true;
    }

    return Gtk::ScrolledWindow::on_button_release_event(e);
}

bool ImageBox::on_motion_notify_event(GdkEventMotion* e)
{
    if (m_Image && ((e->state & GDK_BUTTON1_MASK) == GDK_BUTTON1_MASK ||
                    (e->state & GDK_BUTTON2_MASK) == GDK_BUTTON2_MASK))
    {
        m_Fixed->get_window()->set_cursor(m_FleurCursor);
        scroll(m_PreviousX - e->x_root, m_PreviousY - e->y_root, true);

        m_PreviousX = e->x_root;
        m_PreviousY = e->y_root;

        return true;
    }
    else
    {
        cursor_timeout();
    }

    return Gtk::ScrolledWindow::on_motion_notify_event(e);
}

// Override default scroll behavior to provide interpolated scrolling
bool ImageBox::on_scroll_event(GdkEventScroll* e)
{
    bool handled{ e->delta_x != 0 || e->delta_y != 0 || e->direction != GDK_SCROLL_SMOOTH };
    grab_focus();
    cursor_timeout();

    // Scroll down
    if (e->delta_y > 0 || e->direction == GDK_SCROLL_DOWN)
    {
        if ((e->state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK)
            on_zoom_out();
        else
            scroll(0, 300);
    }
    // Scroll up
    else if (e->delta_y < 0 || e->direction == GDK_SCROLL_UP)
    {
        if ((e->state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK)
            on_zoom_in();
        else
            scroll(0, -300);
    }

    // Scroll right
    if (e->delta_x > 0 || e->direction == GDK_SCROLL_RIGHT)
        scroll(300, 0);
    // Scroll left
    else if (e->delta_x < 0 || e->direction == GDK_SCROLL_LEFT)
        scroll(-300, 0);

    return handled || Gtk::ScrolledWindow::on_scroll_event(e);
}

// This must be called after m_Orig(Width/Height) are set
// It sets the values of w, h to their scaled values, and x, y to center
// coordinates for m_Fixed to use
void ImageBox::get_scale_and_position(int& w, int& h, int& x, int& y)
{
    int ww, wh;
    m_MainWindow->get_drawable_area_size(ww, wh);

    w = m_OrigWidth;
    h = m_OrigHeight;

    double window_aspect = static_cast<double>(ww) / wh, image_aspect = static_cast<double>(w) / h;

    // These do not take the scrollbar size in to account, because I assume that
    // overlay scrollbars are enabled
    if (w > ww && (m_ZoomMode == ZoomMode::FIT_WIDTH ||
                   (m_ZoomMode == ZoomMode::AUTO_FIT && window_aspect <= image_aspect)))
    {
        w = ww;
        h = std::ceil(w / image_aspect);
    }
    else if (h > wh && (m_ZoomMode == ZoomMode::FIT_HEIGHT ||
                        (m_ZoomMode == ZoomMode::AUTO_FIT && window_aspect >= image_aspect)))
    {
        h = wh;
        w = std::ceil(h * image_aspect);
    }
    else if (m_ZoomMode == ZoomMode::MANUAL && m_ZoomPercent != 100)
    {
        w *= static_cast<double>(m_ZoomPercent) / 100;
        h *= static_cast<double>(m_ZoomPercent) / 100;
    }

    x = std::max(0, (ww - w) / 2);
    y = std::max(0, (wh - h) / 2);
}

void ImageBox::draw_image(bool scroll)
{
    // Don't draw images that don't exist (obviously)
    // Don't draw loading animated GIFs
    // Don't draw loading images that haven't created a pixbuf yet
    // Don't draw loading webm files (only booru images can have a loading webm)
    // The pixbuf_changed signal will fire when the above images are ready to be drawn
    if (!m_Image || (m_Image->is_loading() &&
                     (m_Image->is_animated_gif() || !m_Image->get_pixbuf() || m_Image->is_webm())))
    {
        m_RedrawQueued = false;
        return;
    }

    // Temporary pixbuf used when scaling is needed
    Glib::RefPtr<Gdk::Pixbuf> temp_pixbuf;
    bool error{ false };

    // if the image is still loading we want to draw all requests
    // Only booru images will do this
    m_Loading = m_Image->is_loading();

#ifdef HAVE_GSTREAMER
    if (m_Image->is_webm())
    {
        if (!m_Playing)
        {
            g_object_set(
                m_Playbin, "uri", Glib::filename_to_uri(m_Image->get_path()).c_str(), NULL);
            on_set_volume();

            gst_element_set_state(m_Playbin, GST_STATE_PAUSED);
            // Wait for the above changes to actually happen
            gst_element_get_state(m_Playbin, nullptr, nullptr, GST_SECOND * 5);
        }

        GstPad* pad{ nullptr };
        GstCaps* caps{ nullptr };
        g_signal_emit_by_name(m_Playbin, "get-video-pad", 0, &pad, NULL);

        if (pad)
            caps = gst_pad_get_current_caps(pad);

        if (pad && caps)
        {
            GstStructure* s{ gst_caps_get_structure(caps, 0) };

            gst_structure_get_int(s, "width", &m_OrigWidth);
            gst_structure_get_int(s, "height", &m_OrigHeight);

            gst_caps_unref(caps);
            gst_object_unref(pad);
        }
        else
        {
            error = true;
            m_GstWidget->hide();

            if (pad)
                gst_object_unref(pad);
        }
    }
    else
#endif // HAVE_GSTREAMER
    {
        // Start animation if this is a new animated GIF
        if (m_Image->is_animated_gif() && !m_Loading && !m_AnimConn)
        {
            m_AnimConn.disconnect();
            if (!m_Image->get_gif_finished_looping())
                m_AnimConn = Glib::signal_timeout().connect(
                    sigc::mem_fun(*this, &ImageBox::update_animation),
                    m_Image->get_gif_frame_delay());
        }

        Glib::RefPtr<Gdk::Pixbuf> pixbuf = m_Image->get_pixbuf();

        if (pixbuf)
        {
            // Set this here incase we dont need to scale
            temp_pixbuf = pixbuf;

            m_OrigWidth  = pixbuf->get_width();
            m_OrigHeight = pixbuf->get_height();
        }
        else
        {
            error = true;
        }
    }

    if (error)
    {
        temp_pixbuf  = Image::get_missing_pixbuf();
        m_OrigWidth  = temp_pixbuf->get_width();
        m_OrigHeight = temp_pixbuf->get_height();
    }

    int w, h, x, y;
    get_scale_and_position(w, h, x, y);
    m_Scale =
        m_ZoomMode == ZoomMode::MANUAL ? m_ZoomPercent : static_cast<double>(w) / m_OrigWidth * 100;
    if (!m_Image->is_webm() && !error && (w != m_OrigWidth || h != m_OrigHeight))
        temp_pixbuf = m_Image->get_pixbuf()->scale_simple(w, h, Gdk::INTERP_BILINEAR);

    double h_adjust_val{ 0 }, v_adjust_val{ 0 };

    // Used to keep the adjustments centered when manual zooming
    if (m_ZoomScroll)
    {
        h_adjust_val =
            get_hadjustment()->get_value() /
            std::max(get_hadjustment()->get_upper() - get_hadjustment()->get_page_size(), 1.0);
        v_adjust_val =
            get_vadjustment()->get_value() /
            std::max(get_vadjustment()->get_upper() - get_vadjustment()->get_page_size(), 1.0);
    }

    get_window()->freeze_updates();

    m_Fixed->set_size_request(w, h);

    if (temp_pixbuf)
    {
        m_Fixed->move(*m_Overlay, x, y);
        m_GtkImage->set(temp_pixbuf);
        m_Overlay->show();
    }
#ifdef HAVE_GSTREAMER
    else
    {
        if (!m_Playing)
        {
            m_GtkImage->clear();
            m_Overlay->hide();
            m_GstWidget->show();
        }

        m_Fixed->move(*m_GstWidget, x, y);
        m_GstWidget->set_size_request(w, h);
#ifndef _WIN32
        if (m_UsingWayland)
        {
            // waylandsink needs coordinates relative to the main window
            int wx{ 0 }, wy{ 0 };
            auto* toplevel = get_toplevel();
            m_Fixed->translate_coordinates(*toplevel, 0, 0, wx, wy);
            gst_video_overlay_set_render_rectangle(
                GST_VIDEO_OVERLAY(m_VideoSink), wx + x, wy + y, w, h);
        }
        else
        {
            gst_video_overlay_set_render_rectangle(GST_VIDEO_OVERLAY(m_VideoSink), 0, 0, w, h);
            gst_video_overlay_expose(GST_VIDEO_OVERLAY(m_VideoSink));
        }
#endif // !_WIN32
    }
#endif // HAVE_GSTREAMER

    // Reset the scrollbar positions
    if (scroll || m_FirstDraw)
    {
        if (m_RestoreScrollPos.v != -1 && m_RestoreScrollPos.zoom == m_ZoomMode)
        {
            // Restore and reset stored scrollbar positions
            get_vadjustment()->set_value(m_RestoreScrollPos.v);
            get_hadjustment()->set_value(m_RestoreScrollPos.h);
            m_RestoreScrollPos = { -1, -1, m_ZoomMode };
        }
        else
        {
            get_vadjustment()->set_value(0);
            if (Settings.get_bool("MangaMode"))
                // Start at the right side of the image
                get_hadjustment()->set_value(get_hadjustment()->get_upper() -
                                             get_hadjustment()->get_page_size());
            else
                get_hadjustment()->set_value(0);
        }

        if (m_FirstDraw && !m_Image->get_notes().empty() && !m_NotesConn)
            on_notes_changed();

        m_FirstDraw = false;
    }
    else if (m_ZoomScroll)
    {
        get_hadjustment()->set_value(std::max(
            h_adjust_val * (get_hadjustment()->get_upper() - get_hadjustment()->get_page_size()),
            0.0));
        get_vadjustment()->set_value(std::max(
            v_adjust_val * (get_vadjustment()->get_upper() - get_vadjustment()->get_page_size()),
            0.0));
        m_ZoomScroll = false;
    }

#ifdef HAVE_GSTREAMER
    // Finally start playing the video
    if (!error && !m_Playing && m_Image->is_webm())
    {
        gst_element_set_state(m_Playbin, GST_STATE_PLAYING);
        m_Playing = true;
    }
#endif // HAVE_GSTREAMER

    get_window()->thaw_updates();
    m_RedrawQueued = false;
    m_StatusBar->set_resolution(m_OrigWidth, m_OrigHeight, m_Scale, m_ZoomMode);

    update_notes();
    m_SignalImageDrawn();
}

bool ImageBox::update_animation()
{
    if (m_Image->is_loading())
        return true;

    bool gif_finished = m_Image->gif_advance_frame();

    if (!gif_finished)
        m_AnimConn = Glib::signal_timeout().connect(
            sigc::mem_fun(*this, &ImageBox::update_animation), m_Image->get_gif_frame_delay());

    return false;
}

void ImageBox::scroll(const int x, const int y, const bool panning, const bool from_slideshow)
{
    int adjust_upper_x = std::max(0,
                                  static_cast<int>((get_hadjustment()->get_upper()) -
                                                   get_hadjustment()->get_page_size())),
        adjust_upper_y = std::max(0,
                                  static_cast<int>((get_vadjustment()->get_upper()) -
                                                   get_vadjustment()->get_page_size()));

    if (!from_slideshow)
        reset_slideshow();

    if (panning)
    {
        int n_x = get_hadjustment()->get_value() + x, n_y = get_vadjustment()->get_value() + y;

        if (n_x <= adjust_upper_x)
            get_hadjustment()->set_value(n_x);

        if (n_y <= adjust_upper_y)
            get_vadjustment()->set_value(n_y);
#ifdef HAVE_GSTREAMER
        if (m_Playing && m_Image->is_webm())
            gst_video_overlay_expose(GST_VIDEO_OVERLAY(m_VideoSink));
#endif // HAVE_GSTREAMER
    }
    else
    {
        // Rounds scroll amount up if the end/start of the adjustment is
        // within 50% of the initial scroll amount
        // adjv = adjustment value, adjmax = adjustment max value, v = scroll value
        static auto round_scroll = [](const int adjv, const int adjmax, const int v) {
            const int range = std::abs(v * 0.5);
            // Going down/right
            if (v > 0 && adjmax - (adjv + v) <= range)
                return adjmax - adjv;
            // Going up/left, and not already going to scroll to the end
            else if (adjv > std::abs(v) && adjv + v <= range)
                return -adjv;

            return v;
        };

        // Scroll to next page
        if ((get_hadjustment()->get_value() == adjust_upper_x && x > 0) ||
            (get_vadjustment()->get_value() == adjust_upper_y && y > 0))
        {
            if (m_SlideshowConn && !m_NextAction->is_sensitive())
                m_SignalSlideshowEnded();
            else
                m_NextAction->activate();
        }
        // Scroll to previous page
        else if ((get_hadjustment()->get_value() == 0 && x < 0) ||
                 (get_vadjustment()->get_value() == 0 && y < 0))
        {
            m_PreviousAction->activate();
        }
        else if (x != 0)
        {
            smooth_scroll(round_scroll(get_hadjustment()->get_value(), adjust_upper_x, x),
                          m_HSmoothScroll);
        }
        else if (y != 0)
        {
            smooth_scroll(round_scroll(get_vadjustment()->get_value(), adjust_upper_y, y),
                          m_VSmoothScroll);
        }
    }
}

void ImageBox::smooth_scroll(const int amount, ImageBox::SmoothScroll& ss)
{
    // Start a new callback if changing directions or theres no active scrolling callback
    auto target{ ss.end - ss.start };
    if (!ss.scrolling || (amount < 0 && target > 0) || (amount > 0 && target < 0))
    {
        remove_scroll_callback(ss);
        ss.start = ss.adj->get_value();
        ss.end   = ss.start + amount;
    }
    // Continuing an anmation by adding the remaining amount to the current target
    else
    {
        ss.start = ss.adj->get_value();
        ss.end += amount;
    }

    ss.start_time = get_frame_clock()->get_frame_time();
    ss.end_time =
        ss.start_time + std::min(500, static_cast<int>(std::abs(ss.end - ss.start))) * 1000;

    if (!ss.scrolling)
    {
        ss.id = add_tick_callback([&ss](const Glib::RefPtr<Gdk::FrameClock>& clock) {
            auto now{ clock->get_frame_time() };
            if (now < ss.end_time && ss.adj->get_value() != ss.end)
            {
                auto t{ static_cast<double>(now - ss.start_time) / (ss.end_time - ss.start_time) };
                ss.adj->set_value((ss.end - ss.start) * std::sin(t * (M_PI / 2)) + ss.start);
                return (ss.scrolling = true);
            }
            else
            {
                ss.adj->set_value(ss.end);
                return (ss.scrolling = false);
            }
        });
    }
}

void ImageBox::remove_scroll_callback(ImageBox::SmoothScroll& ss)
{
    if (ss.scrolling)
    {
        remove_tick_callback(ss.id);
        ss.scrolling = false;
    }
}

void ImageBox::zoom(const std::uint32_t percent)
{
    if (m_ZoomMode != ZoomMode::MANUAL || percent < 10 || percent > 400)
        return;

    m_ZoomScroll  = m_ZoomPercent != percent;
    m_ZoomPercent = percent;
    queue_draw_image();
}

bool ImageBox::advance_slideshow()
{
    // TODO: Smart scrolling, as an option
    // e.g. Scroll the width of the image before scrolling down
    scroll(0, 300, false, true);

    return true;
}

bool ImageBox::on_cursor_timeout()
{
    m_Fixed->get_window()->set_cursor(m_BlankCursor);

    return false;
}

void ImageBox::on_notes_changed()
{
    double scale{ m_Scale / 100 };
    for (auto& note : m_Image->get_notes())
    {
        auto n{ Gtk::make_managed<ImageBoxNote>(note) };
        m_Notes.push_back(n);

        n->set_scale(scale);
        m_NoteFixed->put(*n, n->get_x(), n->get_y());
        n->show();
    }
}

void ImageBox::clear_notes()
{
    for (auto& note : m_Notes)
        m_NoteFixed->remove(*note);

    m_Notes.clear();
}

void ImageBox::update_notes()
{
    double scale{ m_Scale / 100 };
    for (auto& note : m_Notes)
    {
        note->set_scale(scale);
        m_NoteFixed->move(*note, note->get_x(), note->get_y());
    }
}
