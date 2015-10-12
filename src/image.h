#ifndef _IMAGE_H_
#define _IMAGE_H_

#include <gdkmm.h>
#include <glibmm.h>

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
        virtual ~Image() = default;

        static bool is_valid(const std::string &path);
        static bool is_valid_extension(const std::string &path);
        static const Glib::RefPtr<Gdk::Pixbuf>& get_missing_pixbuf();

        const std::string get_path() const { return m_Path; }
        bool is_loading() const { return m_Loading; }
        bool is_webm() const { return m_isWebM; }

        virtual std::string get_filename() const;
        virtual const Glib::RefPtr<Gdk::PixbufAnimation>& get_pixbuf();
        virtual const Glib::RefPtr<Gdk::Pixbuf>& get_thumbnail();

        virtual void load_pixbuf();
        virtual void reset_pixbuf();

        Glib::Dispatcher& signal_pixbuf_changed() { return m_SignalPixbufChanged; }

        static std::uint8_t const ThumbnailSize = 100;
    protected:
        static bool is_webm(const std::string&);

        void create_thumbnail();
        Glib::RefPtr<Gdk::Pixbuf> create_pixbuf_at_size(const std::string &path,
                                                        const int w, const int h) const;

        bool m_Loading, m_isWebM;
        std::string m_Path, m_ThumbnailPath;

        Glib::RefPtr<Gdk::Pixbuf> m_ThumbnailPixbuf;
        Glib::RefPtr<Gdk::PixbufAnimation> m_Pixbuf;

        Glib::Threads::Mutex m_Mutex;
        Glib::Dispatcher m_SignalPixbufChanged;
    private:
        Glib::RefPtr<Gdk::Pixbuf> scale_pixbuf(Glib::RefPtr<Gdk::Pixbuf> &pixbuf,
                                               const int w, const int h) const;
        Glib::RefPtr<Gdk::Pixbuf> create_webm_thumbnail(const int w, const int h) const;
        Glib::RefPtr<Gdk::Pixbuf> create_webm_thumbnail(const int w, const int h,
                                                        int &oWidth, int &oHeight) const;
#ifdef HAVE_GSTREAMER
        static Glib::RefPtr<Gdk::Pixbuf> capture_frame(GstElement *playbin, const int w,
                                                       int &oWidth, int &oHeight);
        static bool is_pixbuf_interesting(Glib::RefPtr<Gdk::Pixbuf> &pixbuf);

        static constexpr double BoringImageVariance = 256.0;
#endif // HAVE_GSTREAMER
        void create_save_thumbnail();
        void save_thumbnail(Glib::RefPtr<Gdk::Pixbuf> &pixbuf,
                            const int w, const int h, const gchar *mimeType) const;
        static std::string ThumbnailDir;
    };
}

#endif /* _IMAGE_H_ */
