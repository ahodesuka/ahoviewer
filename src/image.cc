#include <iostream>

#include "image.h"
using namespace AhoViewer;

#include "settings.h"

std::string Image::ThumbnailDir = Glib::build_filename(Glib::get_user_cache_dir(), "thumbnails", "normal");

bool Image::is_valid(const std::string &path)
{
    return gdk_pixbuf_get_file_info(path.c_str(), 0, 0) != NULL || is_webm(path);
}

#ifdef HAVE_GSTREAMER
bool Image::is_webm(const std::string &path)
#else
bool Image::is_webm(const std::string&)
#endif // HAVE_GSTREAMER
{
#ifdef HAVE_GSTREAMER
    bool uncertain;
    std::string ct       = Gio::content_type_guess(path, "", uncertain);
    std::string mimeType = Gio::content_type_get_mime_type(ct);

    return mimeType == "video/webm";
#else
    return false;
#endif // HAVE_GSTREAMER
}

const Glib::RefPtr<Gdk::Pixbuf>& Image::get_missing_pixbuf()
{
    static const Glib::RefPtr<Gdk::Pixbuf> pixbuf =
        Gtk::Invisible().render_icon(Gtk::Stock::MISSING_IMAGE, Gtk::ICON_SIZE_DIALOG);

    return pixbuf;
}

Image::Image(const std::string &path, const std::string &thumb_path)
  : m_Loading(false),
    m_isWebM(Image::is_webm(path)),
    m_Path(path),
    m_ThumbnailPath(thumb_path)
{

}

std::string Image::get_filename() const
{
    return Glib::build_filename(
            Glib::path_get_basename(Glib::path_get_dirname(m_Path)),
            Glib::path_get_basename(m_Path));
}

const Glib::RefPtr<Gdk::PixbufAnimation>& Image::get_pixbuf()
{
    Glib::Threads::Mutex::Lock lock(m_Mutex);
    return m_Pixbuf;
}

const Glib::RefPtr<Gdk::Pixbuf>& Image::get_thumbnail()
{
    if (m_ThumbnailPixbuf)
        return m_ThumbnailPixbuf;

#ifdef __linux__
    std::string thumbFilename = Glib::Checksum::compute_checksum(
            Glib::Checksum::CHECKSUM_MD5, Glib::filename_to_uri(m_Path)) + ".png";
    m_ThumbnailPath = Glib::build_filename(ThumbnailDir, thumbFilename);

    if (is_valid(m_ThumbnailPath))
    {
        Glib::RefPtr<Gdk::Pixbuf> pixbuf;
        try
        {
            pixbuf = Gdk::Pixbuf::create_from_file(m_ThumbnailPath, ThumbnailSize, ThumbnailSize);
        }
        catch (const Gdk::PixbufError &ex)
        {
            std::cerr << "Error while loading thumbnail " << m_ThumbnailPath
                      << " for " << get_filename() << ": "<< std::endl
                      << "  " << ex.what() << std::endl;
        }

        if (pixbuf)
        {
            struct stat fileInfo;
            std::string s = pixbuf->get_option("tEXt::Thumb::MTime");

            if (!s.empty())
            {
                time_t mtime = strtol(s.c_str(), nullptr, 10);

                if ((stat(m_Path.c_str(), &fileInfo) == 0) && fileInfo.st_mtime == mtime)
                    m_ThumbnailPixbuf = pixbuf;
            }
        }
    }
#endif // __linux__

    if (!m_ThumbnailPixbuf)
        create_save_thumbnail();

    return m_ThumbnailPixbuf;
}

void Image::load_pixbuf()
{
    if (!m_Pixbuf && !m_isWebM)
    {
        Glib::RefPtr<Gdk::PixbufAnimation> p = Gdk::PixbufAnimation::create_from_file(m_Path);
        {
            Glib::Threads::Mutex::Lock lock(m_Mutex);
            m_Pixbuf = p;
        }
        m_SignalPixbufChanged();
    }
}

void Image::reset_pixbuf()
{
    Glib::Threads::Mutex::Lock lock(m_Mutex);
    m_Pixbuf.reset();
}

void Image::create_thumbnail()
{
    if (m_isWebM)
    {
        m_ThumbnailPixbuf = create_webm_thumbnail(ThumbnailSize, ThumbnailSize);
    }
    else
    {
        try
        {
            m_ThumbnailPixbuf = create_pixbuf_at_size(m_Path, ThumbnailSize, ThumbnailSize);
        }
        catch (const Gdk::PixbufError &ex)
        {
            std::cerr << "Error while creating thumbnail for " << get_filename()
                      << ": " << std::endl << "  " << ex.what() << std::endl;
            m_ThumbnailPixbuf = get_missing_pixbuf();
        }
    }
}

// FIXME: https://bugzilla.gnome.org/show_bug.cgi?id=735422 workaround
Glib::RefPtr<Gdk::Pixbuf> Image::create_pixbuf_at_size(const std::string &path,
                                                       const int w, const int h) const
{
    Glib::RefPtr<Gdk::Pixbuf> pixbuf = Gdk::Pixbuf::create_from_file(path);
    return scale_pixbuf(pixbuf, w, h);
}

Glib::RefPtr<Gdk::Pixbuf> Image::scale_pixbuf(Glib::RefPtr<Gdk::Pixbuf> &pixbuf,
                                              const int w, const int h) const
{
    double r = std::min(static_cast<double>(w) / pixbuf->get_width(),
                        static_cast<double>(h) / pixbuf->get_height());

    return pixbuf->scale_simple(std::max(pixbuf->get_width() * r, 20.0),
                                std::max(pixbuf->get_height() * r, 20.0),
                                Gdk::INTERP_BILINEAR);
}

Glib::RefPtr<Gdk::Pixbuf> Image::create_webm_thumbnail(const int w, const int h) const
{
    int ww, hh;
    return create_webm_thumbnail(w, h, ww, hh);
}

#ifdef HAVE_GSTREAMER
Glib::RefPtr<Gdk::Pixbuf> Image::create_webm_thumbnail(const int w, const int h,
                                                       int &oWidth, int &oHeight) const
#else
Glib::RefPtr<Gdk::Pixbuf> Image::create_webm_thumbnail(const int, const int,
                                                       int&, int&) const
#endif // HAVE_GSTREAMER
{
    Glib::RefPtr<Gdk::Pixbuf> pixbuf;
#ifdef HAVE_GSTREAMER
    GstElement *playbin   = gst_element_factory_make("playbin", NULL),
               *audiosink = gst_element_factory_make("fakesink", NULL),
               *videosink = gst_element_factory_make("fakesink", NULL);

    g_object_set(playbin,
                 "uri", Glib::filename_to_uri(m_Path).c_str(),
                 "audio-sink", audiosink,
                 "video-sink", videosink,
                 NULL);

    gst_element_set_state(playbin, GST_STATE_PAUSED);
    gst_element_get_state(playbin, NULL, NULL, GST_CLOCK_TIME_NONE);

    gint64 dur = -1;
    if (gst_element_query_duration(playbin, GST_FORMAT_TIME, &dur) && dur != -1)
        dur /= GST_MSECOND;

    if (dur == -1)
    {
        pixbuf = capture_frame(playbin, w, oWidth, oHeight);
    }
    else
    {
        std::vector<double> offsets = { 1.0 / 3.0, 2.0 / 3.0, 0.1, 0.9, 0.5 };

        for (const double offset : offsets)
        {
            gst_element_seek_simple(playbin, GST_FORMAT_TIME,
                                    static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
                                    offset * dur * GST_MSECOND);

            gst_element_get_state(playbin, NULL, NULL, GST_CLOCK_TIME_NONE);

            if (!(pixbuf = capture_frame(playbin, w, oWidth, oHeight)))
                continue;

            if (is_pixbuf_interesting(pixbuf))
                break;
        }
    }

    gst_element_set_state(playbin, GST_STATE_NULL);
    g_object_unref(playbin);

    if (pixbuf)
        pixbuf = scale_pixbuf(pixbuf, w, h);
#endif // HAVE_GSTREAMER
    return pixbuf;
}

#ifdef HAVE_GSTREAMER
Glib::RefPtr<Gdk::Pixbuf> Image::capture_frame(GstElement *playbin, const int w,
                                               int &oWidth, int &oHeight) const
{
    Glib::RefPtr<Gdk::Pixbuf> pixbuf;

    GstCaps *to_caps = gst_caps_new_simple("video/x-raw",
                                           "format", G_TYPE_STRING, "RGB",
                                           "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
                                           "width", G_TYPE_INT, w,
                                           NULL);

    GstSample *sample = nullptr;
    g_signal_emit_by_name(playbin, "convert-sample", to_caps, &sample);
    gst_caps_unref(to_caps);

    if (sample)
    {
        GstCaps *sample_caps = gst_sample_get_caps(sample);

        if (sample_caps)
        {
            oWidth = 0, oHeight = 0;
            GstStructure *s = gst_caps_get_structure(sample_caps, 0);
            gst_structure_get_int(s, "width", &oWidth);
            gst_structure_get_int(s, "height", &oHeight);

            if (oWidth > 0 && oHeight > 0)
            {
                GstMapInfo info;
                GstMemory *mem = gst_buffer_get_memory(gst_sample_get_buffer(sample), 0);
                if (gst_memory_map(mem, &info, GST_MAP_READ))
                {
                    pixbuf = Gdk::Pixbuf::create_from_data(info.data,
                                                           Gdk::COLORSPACE_RGB, false, 8,
                                                           oWidth, oHeight,
                                                           GST_ROUND_UP_4(w * 3));

                    gst_memory_unmap(mem, &info);
                }

                gst_memory_unref(mem);
            }
        }

        gst_sample_unref(sample);
    }

    return pixbuf;
}

bool Image::is_pixbuf_interesting(Glib::RefPtr<Gdk::Pixbuf> &pixbuf) const
{
    size_t len      = pixbuf->get_rowstride() * pixbuf->get_height();
    guint8 *buf     = pixbuf->get_pixels();
    double xbar     = 0.0,
           variance = 0.0;

    for (size_t i = 0; i < len; ++i)
        xbar += static_cast<double>(buf[i]);
    xbar /= static_cast<double>(len);

    for (size_t i = 0; i < len; ++i)
        variance += std::pow(static_cast<double>(buf[i]) - xbar, 2);

    return variance > BoringImageVariance;
}
#endif // HAVE_GSTREAMER

void Image::create_save_thumbnail()
{
#ifdef __linux__
    int w, h;
    Glib::RefPtr<Gdk::Pixbuf> pixbuf;

    if (m_isWebM)
    {
        pixbuf = create_webm_thumbnail(128, 128, w, h);

        if (pixbuf)
            save_thumbnail(pixbuf, w, h, "video/webm");
    }
    else
    {
        GdkPixbufFormat *format = gdk_pixbuf_get_file_info(m_Path.c_str(), &w, &h);
        if (format && (w > 128 || h > 128))
        {
            try
            {
                pixbuf = create_pixbuf_at_size(m_Path, 128, 128);
            }
            catch (const Gdk::PixbufError &ex)
            {
                std::cerr << "Error while creating thumbnail for " << get_filename()
                          << ": " << std::endl << "  " << ex.what() << std::endl;
                m_ThumbnailPixbuf = get_missing_pixbuf();
                return;
            }

            gchar **mimeTypes = gdk_pixbuf_format_get_mime_types(format);

            save_thumbnail(pixbuf, w, h, mimeTypes[0]);
            g_strfreev(mimeTypes);
        }
    }

    if (pixbuf)
        m_ThumbnailPixbuf = scale_pixbuf(pixbuf, ThumbnailSize, ThumbnailSize);
    else
#endif // __linux__
        create_thumbnail();
}

void Image::save_thumbnail(Glib::RefPtr<Gdk::Pixbuf> &pixbuf,
                           const int w, const int h, const gchar *mimeType) const
{
    if (!Settings.get_bool("SaveThumbnails"))
        return;

    struct stat fileInfo;
    if (stat(m_Path.c_str(), &fileInfo) == 0)
    {
        std::vector<std::string> opts =
        {
            "tEXt::Thumb::URI",
            "tEXt::Thumb::MTime",
            "tEXt::Thumb::Size",
            "tEXt::Thumb::Image::Width",
            "tEXt::Thumb::Image::Height",
            "tEXt::Thumb::Image::Mimetype",
            "tEXt::Software"
        },
        vals =
        {
            Glib::filename_to_uri(m_Path),      // URI
            std::to_string(fileInfo.st_mtime),  // MTime
            std::to_string(fileInfo.st_size),   // Size
            std::to_string(w),                  // Width
            std::to_string(h),                  // Height
            mimeType,                           // Mimetype
            PACKAGE_STRING                      // Software
        };

        if (!Glib::file_test(ThumbnailDir, Glib::FILE_TEST_EXISTS))
            g_mkdir_with_parents(ThumbnailDir.c_str(), 0700);

        gchar *buf;
        gsize bufSize;
        pixbuf->save_to_buffer(buf, bufSize, "png", opts, vals);

        if (bufSize > 0)
        {
            try
            {
                Glib::file_set_contents(m_ThumbnailPath, buf, bufSize);
                chmod(m_ThumbnailPath.c_str(), S_IRUSR | S_IWUSR);
            }
            catch (const Glib::FileError &ex)
            {
                std::cerr << "Glib::file_set_contents: " << ex.what() << std::endl;
            }
        }

        g_free(buf);
    }
}
