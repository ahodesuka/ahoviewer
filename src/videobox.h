#pragma once

#include "config.h"

#include <gtkmm.h>

#ifdef HAVE_GSTREAMER
#include <gst/gst.h>
#endif // HAVE_GSTREAMER

namespace AhoViewer
{

    class VideoBox : public Gtk::Overlay
    {
    public:
        VideoBox(BaseObjectType*, const Glib::RefPtr<Gtk::Builder>&);
        ~VideoBox() override;

        void show();
        void hide();
        std::tuple<bool, int, int> load_file(const std::string& path);
        void start();

#ifdef HAVE_GSTREAMER
        bool is_playing() const { return m_Playing; };
#endif // HAVE_GSTREAMER

        bool hide_controls(bool skip_timeout = false);

    private:
        Gtk::Revealer* m_ControlRevealer;
        Gtk::Button *m_PlayPauseButton, *m_VolumeButton;
        Gtk::Label *m_PosLabel, *m_DurLabel;
        Gtk::Scale *m_VolumeScale, *m_SeekScale;
#ifdef HAVE_GSTREAMER
        Gtk::Widget* m_GstWidget;

        static gboolean bus_cb(GstBus*, GstMessage* msg, void* userp);
        static void on_about_to_finish(GstElement*, void* userp);

        void show_controls();

        bool enter_notify_event(GdkEventCrossing*);
        bool leave_notify_event(GdkEventCrossing*);

        void toggle_play_state();
        void toggle_volume_button();
        void update_volume_button();
        void on_volume_changed();
        bool on_seek_button_press(GdkEventButton* e);
        bool on_seek_button_release(GdkEventButton* e);

        void play();
        void pause();

        void set_volume();
        void reset_gstreamer_pipeline();
        void start_update_controls();
        bool update_controls();

        GstElement* m_Playbin;
        GstState m_State{ GST_STATE_NULL };
        bool m_Playing{ false }, m_Prerolled{ false }, m_Mute{ false }, m_Seeking{ false },
            m_ShowControls{ false }, m_HoveringControls{ false };

        sigc::connection m_UpdateConn, m_HideConn, m_VolumeConn;
#endif // HAVE_GSTREAMER
    };
}
