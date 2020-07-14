#ifndef _IMAGE_H_
#define _IMAGE_H_

#include <gdkmm.h>
#include <glibmm.h>

extern "C" {
#include <libnsgif.h>
}

#include <atomic>
#include <mutex>
#include <thread>

#include "config.h"

#ifdef HAVE_GSTREAMER
#include <gst/gst.h>
#endif // HAVE_GSTREAMER

namespace AhoViewer
{
    class Image
    {
    public:
        Image(const std::string &path);
        virtual ~Image();

        static bool is_valid(const std::string &path);
        static bool is_valid_extension(const std::string &path);
        static const Glib::RefPtr<Gdk::Pixbuf>& get_missing_pixbuf();

        const std::string get_path() const { return m_Path; }
        bool is_webm() const { return m_isWebM; }
        bool is_animated_gif() const { return m_GIFanim && m_GIFanim->frame_count > 1; }

        // This is used to let the imagebox know that load_pixbuf has been or needs to be
        // called but has not yet finished loading.  When the image has finished loading
        // and get_pixbuf() returns a nullptr the imagebox will show the missing pixbuf
        // webm files will always return false as gstreamer will determine whether they are valid or not
        virtual bool is_loading() const { return !m_isWebM && m_Loading; }
        virtual std::string get_filename() const;
        virtual const Glib::RefPtr<Gdk::Pixbuf>& get_pixbuf();
        virtual const Glib::RefPtr<Gdk::Pixbuf>& get_thumbnail(Glib::RefPtr<Gio::Cancellable> c);

        virtual void load_pixbuf(Glib::RefPtr<Gio::Cancellable> c);
        virtual void reset_pixbuf();

        bool gif_advance_frame();
        bool get_gif_finished_looping() const;
        unsigned int get_gif_frame_delay() const;
        void reset_gif_animation();

        Glib::Dispatcher& signal_pixbuf_changed() { return m_SignalPixbufChanged; }

        static const size_t ThumbnailSize = 100;
    protected:
        static bool is_webm(const std::string&);

        void create_gif_frame_pixbuf();
        bool is_gif(const unsigned char*);
        void create_thumbnail(Glib::RefPtr<Gio::Cancellable> c, bool save = true);
        Glib::RefPtr<Gdk::Pixbuf> create_pixbuf_at_size(const std::string &path,
                                                        const int w, const int h,
                                                        Glib::RefPtr<Gio::Cancellable> c) const;

        bool m_isWebM;
        std::atomic<bool> m_Loading {true};
        std::string m_Path, m_ThumbnailPath;

        Glib::RefPtr<Gdk::Pixbuf> m_ThumbnailPixbuf;
        Glib::RefPtr<Gdk::Pixbuf> m_Pixbuf;

        gif_animation* m_GIFanim {nullptr};
        gif_bitmap_callback_vt m_BitmapCallbacks;
        int m_GIFcurFrame {0}, m_GIFcurLoop {1};

        std::mutex m_Mutex;
        Glib::Dispatcher m_SignalPixbufChanged;
    private:
        Glib::RefPtr<Gdk::Pixbuf> scale_pixbuf(Glib::RefPtr<Gdk::Pixbuf> &pixbuf,
                                               const int w, const int h) const;

        Glib::RefPtr<Gdk::Pixbuf> create_webm_thumbnail(int w, int h) const;
        void save_thumbnail(Glib::RefPtr<Gdk::Pixbuf> &pixbuf, const gchar *mimeType) const;

        static std::string ThumbnailDir;
    };
}

#endif /* _IMAGE_H_ */
