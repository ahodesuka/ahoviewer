#include "videobox.h"
using namespace AhoViewer;

#include "settings.h"

#include <iostream>

#ifdef HAVE_GSTREAMER
#include <gst/audio/streamvolume.h>

gboolean VideoBox::bus_cb(GstBus*, GstMessage* msg, void* userp)
{
    auto* self = static_cast<VideoBox*>(userp);

    switch (GST_MESSAGE_TYPE(msg))
    {
    case GST_MESSAGE_ASYNC_DONE:
        if (!self->m_Prerolled)
        {
            GstSeekFlags flags = GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_SEGMENT);
            gst_element_seek_simple(self->m_Playbin, GST_FORMAT_TIME, flags, 0);
            self->m_Prerolled = true;
            self->start_update_controls();
        }
        break;
    case GST_MESSAGE_SEGMENT_DONE:
        self->m_UpdateConn.block();
        gst_element_seek_simple(self->m_Playbin, GST_FORMAT_TIME, GST_SEEK_FLAG_SEGMENT, 0);
        self->m_UpdateConn.unblock();
        break;
    case GST_MESSAGE_STATE_CHANGED:
        if (GST_MESSAGE_SRC(msg) == GST_OBJECT(self->m_Playbin))
        {
            GstState old_state, new_state;
            gst_message_parse_state_changed(msg, &old_state, &new_state, nullptr);
            self->m_State = new_state;
            if (old_state == GST_STATE_READY && new_state == GST_STATE_PAUSED)
                self->update_controls();
        }
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
#endif // HAVE_GSTREAMER

VideoBox::VideoBox(BaseObjectType* cobj, const Glib::RefPtr<Gtk::Builder>& bldr)
    : Gtk::Overlay{ cobj }
#ifdef HAVE_GSTREAMER
      ,
      m_Mute{ Settings.get_bool("Mute") }
#endif // HAVE_GSTREAMER
{
    bldr->get_widget("VideoBox::ControlRevealer", m_ControlRevealer);
    bldr->get_widget("VideoBox::PlayPauseButton", m_PlayPauseButton);
    bldr->get_widget("VideoBox::VolumeButton", m_VolumeButton);
    bldr->get_widget("VideoBox::VolumeScale", m_VolumeScale);
    bldr->get_widget("VideoBox::PosLabel", m_PosLabel);
    bldr->get_widget("VideoBox::SeekScale", m_SeekScale);
    bldr->get_widget("VideoBox::DurLabel", m_DurLabel);

#ifdef HAVE_GSTREAMER
    // Prefer hardware accelerated decoders if they exist
    const auto hw_decoders = {
        "nvh264dec",       "nvh264sldec",    "nvh265dec",   "nvh265dec",   "nvmpeg2videodec",
        "nvmpeg4videodec", "nvmpegvideodec", "nvvp8dec",    "nvvp9dec",    "vaapih264dec",
        "vaapih264dec",    "vaapimpeg2dec",  "vaapivc1dec",
#ifdef _WIN32
        "d3d11h264dec",    "d3d11h265dec",   "d3d11vp8dec", "d3d11vp9dec",
#endif // _WIN32
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

    GstElement* gtksink{ gst_element_factory_make("gtkglsink", "gtksink") };
    GstElement* glsinkbin{ nullptr };
    if (!gtksink)
    {
        gtksink   = gst_element_factory_make("gtksink", "gtksink");
        glsinkbin = gtksink;
    }
    else
    {
        glsinkbin = gst_element_factory_make("glsinkbin", "glsinkbin");
        g_object_set(glsinkbin, "sink", gtksink, nullptr);
    }

    if (gtksink)
    {
        GtkWidget* w{ nullptr };
        g_object_get(gtksink, "widget", &w, nullptr);
        m_GstWidget = Glib::wrap(w);
        add(*m_GstWidget);
        g_object_unref(w);

        m_GstWidget->set_can_focus(false);
        m_GstWidget->set_events(Gdk::EventMask::POINTER_MOTION_MASK |
                                Gdk::EventMask::ENTER_NOTIFY_MASK |
                                Gdk::EventMask::LEAVE_NOTIFY_MASK);
        m_GstWidget->signal_enter_notify_event().connect(
            sigc::mem_fun(*this, &VideoBox::enter_notify_event));
        m_GstWidget->signal_leave_notify_event().connect(
            sigc::mem_fun(*this, &VideoBox::leave_notify_event));
        m_GstWidget->signal_motion_notify_event().connect([&](GdkEventMotion*) {
            show_controls();
            return false;
        });
        m_ControlRevealer->signal_enter_notify_event().connect([&](GdkEventCrossing* e) {
            m_HoveringControls = true;
            return enter_notify_event(e);
        });
        m_ControlRevealer->signal_leave_notify_event().connect([&](GdkEventCrossing* e) {
            m_HoveringControls = false;
            return leave_notify_event(e);
        });
        m_ControlRevealer->signal_motion_notify_event().connect([&](GdkEventMotion*) {
            m_HoveringControls = true;
            show_controls();
            return false;
        });

        auto audiosink{ Settings.get_string("AudioSink") };
        if (audiosink.empty())
            audiosink = "autoaudiosink";

        // playbin3 will break things.
        Glib::unsetenv("USE_PLAYBIN3");
        m_Playbin = gst_element_factory_make("playbin", "playbin");
        g_object_set(m_Playbin,
                     "audio-sink",
                     gst_element_factory_make(audiosink.c_str(), "audiosink"),
                     "video-sink",
                     glsinkbin,
                     "force-aspect-ratio",
                     false,
                     NULL);

        GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(m_Playbin));
        gst_bus_add_watch(bus, &VideoBox::bus_cb, this);
        g_object_unref(bus);

        m_PlayPauseButton->signal_clicked().connect(
            sigc::mem_fun(*this, &VideoBox::toggle_play_state));
        m_VolumeButton->signal_clicked().connect(
            sigc::mem_fun(*this, &VideoBox::toggle_volume_button));

        m_VolumeScale->set_value(m_Mute ? 0 : Settings.get_int("Volume"));
        update_volume_button();
        m_VolumeConn = m_VolumeScale->signal_value_changed().connect(
            sigc::mem_fun(*this, &VideoBox::on_volume_changed));

        // Ignore all scroll events on the seek scale
        m_SeekScale->signal_scroll_event().connect([](GdkEventScroll*) { return true; }, false);
        m_SeekScale->signal_button_press_event().connect(
            sigc::mem_fun(*this, &VideoBox::on_seek_button_press), false);
        m_SeekScale->signal_button_release_event().connect(
            sigc::mem_fun(*this, &VideoBox::on_seek_button_release), false);

        // Prevent the imagebox from recieving any button events when clicking on the
        // controls
        m_ControlRevealer->signal_button_press_event().connect(
            [](GdkEventButton*) { return true; });
        m_ControlRevealer->signal_button_release_event().connect(
            [](GdkEventButton*) { return true; });
        m_ControlRevealer->signal_scroll_event().connect([](GdkEventScroll*) { return true; });

        auto css = Gtk::CssProvider::create();
        css->load_from_resource("/ui/css/video-controls.css");
        get_style_context()->add_provider_for_screen(
            Gdk::Screen::get_default(), css, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
    else
    {
        std::cerr << "Failed to load gtkglsink or gtksink gstreamer plugin." << std::endl;
    }
#endif // HAVE_GSTREAMER
}

VideoBox::~VideoBox()
{
#ifdef HAVE_GSTREAMER
    reset_gstreamer_pipeline();
    gst_object_unref(GST_OBJECT(m_Playbin));
#endif // HAVE_GSTREAMER
}

void VideoBox::show()
{
    m_ControlRevealer->show();
    Gtk::Bin::show();
}

void VideoBox::hide()
{
#ifdef HAVE_GSTREAMER
    m_UpdateConn.disconnect();
    reset_gstreamer_pipeline();
#endif // HAVE_GSTREAMER

    m_ControlRevealer->hide();
    m_ControlRevealer->set_reveal_child(false);
    Gtk::Bin::hide();
}

std::tuple<bool, int, int> VideoBox::load_file([[maybe_unused]] const std::string& path)
{
    bool error{ false };
    int w{ 0 }, h{ 0 };

#ifdef HAVE_GSTREAMER
    m_UpdateConn.disconnect();
    g_object_set(m_Playbin, "uri", Glib::filename_to_uri(path).c_str(), NULL);
    set_volume();

    gst_element_set_state(m_Playbin, GST_STATE_PAUSED);
    // Wait for the above changes to actually happen
    gst_element_get_state(m_Playbin, nullptr, nullptr, GST_SECOND * 3);

    GstPad* pad{ nullptr };
    GstCaps* caps{ nullptr };
    g_signal_emit_by_name(m_Playbin, "get-video-pad", 0, &pad, NULL);

    if (pad)
        caps = gst_pad_get_current_caps(pad);

    if (pad && caps)
    {
        GstStructure* s{ gst_caps_get_structure(caps, 0) };

        gst_structure_get_int(s, "width", &w);
        gst_structure_get_int(s, "height", &h);

        gst_caps_unref(caps);
        gst_object_unref(pad);

        // Check if the video has an audio track
        g_signal_emit_by_name(m_Playbin, "get-audio-pad", 0, &pad, NULL);

        if (!pad)
        {
            m_VolumeButton->hide();
            m_VolumeScale->hide();
        }
        else
        {
            m_VolumeButton->show();
            m_VolumeScale->show();
            gst_object_unref(pad);
        }
    }
    else
    {
        error = true;
        hide();

        if (pad)
            gst_object_unref(pad);
    }
#endif // HAVE_GSTREAMER

    return { error, w, h };
}

void VideoBox::start()
{
#ifdef HAVE_GSTREAMER
    play();
    m_Playing = true;
#endif // HAVE_GSTREAMER
}

bool VideoBox::hide_controls(bool skip_timeout)
{
#ifdef HAVE_GSTREAMER
    m_HideConn.disconnect();
    // Don't hide if the cursor is currently inside the control widget
    if (m_HoveringControls)
        return false;

    m_HideConn = Glib::signal_timeout().connect(
        [&]() {
            if (!m_ShowControls)
                m_ControlRevealer->set_reveal_child(false);
            return false;
        },
        skip_timeout ? 0 : 500);

    m_ShowControls = false;
#endif // HAVE_GSTREAMER

    return true;
}

#ifdef HAVE_GSTREAMER
void VideoBox::show_controls()
{
    m_HideConn.disconnect();
    m_ControlRevealer->set_reveal_child(true);

    m_ShowControls = true;
}

bool VideoBox::enter_notify_event(GdkEventCrossing*)
{
    show_controls();
    return false;
}

bool VideoBox::leave_notify_event(GdkEventCrossing*)
{
    hide_controls();
    return false;
}

void VideoBox::toggle_play_state()
{
    if (m_State == GST_STATE_PLAYING)
        pause();
    else if (m_State == GST_STATE_PAUSED)
        play();
}

void VideoBox::toggle_volume_button()
{
    m_Mute = !m_Mute;
    Settings.set("Mute", m_Mute);
    g_object_set(m_Playbin, "mute", m_Mute, NULL);

    m_VolumeConn.block();
    if (m_Mute)
    {
        m_VolumeScale->set_value(0);
    }
    else
    {
        m_VolumeScale->set_value(Settings.get_int("Volume"));
        set_volume();
    }
    m_VolumeConn.unblock();

    update_volume_button();
}

void VideoBox::update_volume_button()
{
    int vol{ static_cast<int>(m_VolumeScale->get_value()) };
    if (m_Mute || vol <= 0)
    {
        static_cast<Gtk::Image*>(m_VolumeButton->get_child())
            ->set_from_icon_name("audio-volume-muted-symbolic", Gtk::ICON_SIZE_SMALL_TOOLBAR);
    }
    else if (vol <= 33)
    {
        static_cast<Gtk::Image*>(m_VolumeButton->get_child())
            ->set_from_icon_name("audio-volume-low-symbolic", Gtk::ICON_SIZE_SMALL_TOOLBAR);
    }
    else if (vol <= 66)
    {
        static_cast<Gtk::Image*>(m_VolumeButton->get_child())
            ->set_from_icon_name("audio-volume-medium-symbolic", Gtk::ICON_SIZE_SMALL_TOOLBAR);
    }
    else
    {
        static_cast<Gtk::Image*>(m_VolumeButton->get_child())
            ->set_from_icon_name("audio-volume-high-symbolic", Gtk::ICON_SIZE_SMALL_TOOLBAR);
    }
}

void VideoBox::on_volume_changed()
{
    if (m_Mute)
    {
        m_Mute = false;
        Settings.set("Mute", m_Mute);
        g_object_set(m_Playbin, "mute", m_Mute, NULL);
    }

    Settings.set("Volume", static_cast<int>(m_VolumeScale->get_value()));
    set_volume();
    update_volume_button();
}

bool VideoBox::on_seek_button_press(GdkEventButton* e)
{
    // Only left clicks go through
    if (e->button != 1)
        return true;

    m_Seeking = true;
    return false;
}

bool VideoBox::on_seek_button_release(GdkEventButton* e)
{
    // Only left clicks go through
    if (e->button != 1)
        return true;

    gint64 dur;
    if (gst_element_query_duration(m_Playbin, GST_FORMAT_TIME, &dur))
    {
        GstSeekFlags flags = GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_SEGMENT);
        auto seek_pos{ static_cast<gint64>(std::llround(m_SeekScale->get_value() * dur / 100)) };
        gst_element_seek_simple(m_Playbin, GST_FORMAT_TIME, flags, std::max<gint64>(seek_pos, 0));
    }

    m_Seeking = false;
    return false;
}

void VideoBox::play()
{
    gst_element_set_state(m_Playbin, GST_STATE_PLAYING);
    static_cast<Gtk::Image*>(m_PlayPauseButton->get_child())
        ->set_from_icon_name("media-playback-pause-symbolic", Gtk::ICON_SIZE_SMALL_TOOLBAR);
}

void VideoBox::pause()
{
    gst_element_set_state(m_Playbin, GST_STATE_PAUSED);
    static_cast<Gtk::Image*>(m_PlayPauseButton->get_child())
        ->set_from_icon_name("media-playback-start-symbolic", Gtk::ICON_SIZE_SMALL_TOOLBAR);
}

void VideoBox::set_volume()
{
    gst_stream_volume_set_volume(GST_STREAM_VOLUME(m_Playbin),
                                 GST_STREAM_VOLUME_FORMAT_CUBIC,
                                 std::clamp(m_VolumeScale->get_value() / 100, 0.0, 1.0));
}

void VideoBox::reset_gstreamer_pipeline()
{
    gst_element_set_state(m_Playbin, GST_STATE_NULL);
    m_Playing = m_Prerolled = false;
}

void VideoBox::start_update_controls()
{
    // 33 = 30 updates per second.
    m_UpdateConn =
        Glib::signal_timeout().connect(sigc::mem_fun(*this, &VideoBox::update_controls), 33);
}

bool VideoBox::update_controls()
{
    if (m_State < GST_STATE_PAUSED)
        return true;

    static constexpr auto format_time = [](gint64 time) {
        if (!GST_CLOCK_TIME_IS_VALID(time))
            return std::string{ "" };

        gchar time_str[16];
        guint hours, minutes, seconds;
        hours   = (guint)(((GstClockTime)(time)) / (GST_SECOND * 60 * 60));
        minutes = (guint)((((GstClockTime)(time)) / (GST_SECOND * 60)) % 60);
        seconds = (guint)((((GstClockTime)(time)) / GST_SECOND) % 60);

        if (hours > 0)
            g_snprintf(time_str, 16, "%u:%02u:%02u", hours, minutes, seconds);
        else
            g_snprintf(time_str, 16, "%u:%02u", minutes, seconds);

        return std::string{ time_str };
    };

    gint64 pos, dur;
    if (gst_element_query_position(m_Playbin, GST_FORMAT_TIME, &pos) &&
        gst_element_query_duration(m_Playbin, GST_FORMAT_TIME, &dur))
    {
        m_PosLabel->set_text(format_time(pos));
        m_DurLabel->set_text(format_time(dur));

        if (!m_Seeking && m_State == GST_STATE_PLAYING)
            m_SeekScale->set_value(static_cast<double>(pos) / dur * 100);
    }

    return true;
}
#endif // HAVE_GSTREAMER
