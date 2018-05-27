#include <cctype>
#include <iostream>

#include "image.h"
using namespace AhoViewer;

#include "settings.h"

std::string Image::ThumbnailDir = Glib::build_filename(Glib::get_user_cache_dir(), "thumbnails", "normal");

bool Image::is_valid(const std::string &path)
{
    return gdk_pixbuf_get_file_info(path.c_str(), 0, 0) != NULL || is_webm(path);
}

bool Image::is_valid_extension(const std::string &path)
{
    std::string ext = path.substr(path.find_last_of('.') + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    for (const Gdk::PixbufFormat &i : Gdk::Pixbuf::get_formats())
        for (const std::string &j : i.get_extensions())
            if (ext == j)
                return true;

#ifdef HAVE_GSTREAMER
    if (ext == "webm")
        return true;
#endif // HAVE_GSTREAMER

    return false;
}

bool Image::is_webm(const std::string &path)
{
#ifdef HAVE_GSTREAMER
    bool uncertain;
    std::string ct       = Gio::content_type_guess(path, NULL, 0, uncertain);
    std::string mimeType = Gio::content_type_get_mime_type(ct);

    return mimeType == "video/webm";
#else
    (void)path;
    return false;
#endif // HAVE_GSTREAMER
}

const Glib::RefPtr<Gdk::Pixbuf>& Image::get_missing_pixbuf()
{
    static const Glib::RefPtr<Gdk::Pixbuf> pixbuf =
        Gtk::IconTheme::get_default()->load_icon("image-missing", 48,
            Gtk::ICON_LOOKUP_USE_BUILTIN | Gtk::ICON_LOOKUP_GENERIC_FALLBACK);

    return pixbuf;
}

Image::Image(const std::string &path)
  : m_isWebM(Image::is_webm(path)),
    m_Loading(true),
    m_Path(path)
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
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_Pixbuf;
}

const Glib::RefPtr<Gdk::Pixbuf>& Image::get_thumbnail(Glib::RefPtr<Gio::Cancellable> c)
{
    if (m_ThumbnailPixbuf)
        return m_ThumbnailPixbuf;

#ifdef __linux__
    std::string thumbFilename = Glib::Checksum::compute_checksum(
            Glib::Checksum::CHECKSUM_MD5, Glib::filename_to_uri(m_Path)) + ".png";
    m_ThumbnailPath = Glib::build_filename(ThumbnailDir, thumbFilename);

    if (is_valid(m_ThumbnailPath))
    {
        Glib::RefPtr<Gdk::Pixbuf> pixbuf = create_pixbuf_at_size(m_ThumbnailPath,
                                                    ThumbnailSize, ThumbnailSize, c);

        if (pixbuf)
        {
            struct stat fileInfo;
            std::string s = pixbuf->get_option("tEXt::Thumb::MTime");

            // Make sure the file hasn't been modified since this thumbnail was created
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
        create_thumbnail(c);

    return m_ThumbnailPixbuf;
}

void Image::load_pixbuf(Glib::RefPtr<Gio::Cancellable> c)
{
    if (!m_Pixbuf && !m_isWebM)
    {
        Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(m_Path);
        GdkPixbufAnimation *p = gdk_pixbuf_animation_new_from_stream(
            G_INPUT_STREAM(file->read()->gobj()), c->gobj(), NULL);

        if (c->is_cancelled())
            return;

        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Pixbuf = Glib::wrap(p);
        }

        m_Loading = false;
        m_SignalPixbufChanged();
    }
}

void Image::reset_pixbuf()
{
    m_Loading = true;
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Pixbuf.reset();
}

void Image::create_thumbnail(Glib::RefPtr<Gio::Cancellable> c, bool save)
{
    Glib::RefPtr<Gdk::Pixbuf> pixbuf;
    save = save && Settings.get_bool("SaveThumbnails");

    if (m_isWebM)
    {
        pixbuf = create_webm_thumbnail(128, 128);

#ifdef __linux__
        if (pixbuf && save)
            save_thumbnail(pixbuf, "video/webm");
#endif // __linux__
    }
    else
    {
        pixbuf = create_pixbuf_at_size(m_Path, 128, 128, c);

        if (c->is_cancelled() || !pixbuf)
            return;

#ifdef __linux__
        int w, h;
        GdkPixbufFormat *format = gdk_pixbuf_get_file_info(m_Path.c_str(), &w, &h);

        if (save && format && (w > 128 || h > 128))
        {
            gchar **mimeTypes = gdk_pixbuf_format_get_mime_types(format);
            save_thumbnail(pixbuf, mimeTypes[0]);
            g_strfreev(mimeTypes);
        }
#endif // __linux__
    }

    if (pixbuf && !c->is_cancelled())
        m_ThumbnailPixbuf = scale_pixbuf(pixbuf, ThumbnailSize, ThumbnailSize);
}

Glib::RefPtr<Gdk::Pixbuf> Image::create_pixbuf_at_size(const std::string &path,
                                                       const int w, const int h,
                                                       Glib::RefPtr<Gio::Cancellable> c) const
{
    Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(path);
    Glib::RefPtr<Gdk::Pixbuf> pixbuf;

    try
    {
        pixbuf = Gdk::Pixbuf::create_from_stream_at_scale(file->read(), w, h, true, c);
    }
    catch (...)
    {
        if (!c->is_cancelled())
            std::cerr << "Error while loading thumbnail " << path
                      << " for " << get_filename() << std::endl;
    }

    return pixbuf;
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

Glib::RefPtr<Gdk::Pixbuf> Image::create_webm_thumbnail(int w, int h) const
{
    Glib::RefPtr<Gdk::Pixbuf> pixbuf;
#ifdef HAVE_GSTREAMER
    gint64 dur, pos;
    GstSample *sample;
    GstMapInfo map;

    std::string des = Glib::ustring::compose("uridecodebin uri=%1 ! videoconvert ! videoscale ! "
           "appsink name=sink caps=\"video/x-raw,format=RGB,width=%2,pixel-aspect-ratio=1/1\"",
           Glib::filename_to_uri(m_Path).c_str(), w);
    GError *error = nullptr;
    GstElement *pipeline = gst_parse_launch(des.c_str(), &error);

    if (error != nullptr)
    {
        std::cerr << "create_webm_thumbnail: could not construct pipeline: " << error->message << std::endl;
        g_error_free(error);

        if (pipeline)
            gst_object_unref(pipeline);

        return pixbuf;
    }

    GstElement *sink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");

    gst_element_set_state(pipeline, GST_STATE_PAUSED);
    gst_element_get_state(pipeline, NULL, NULL, 5 * GST_SECOND);
    gst_element_query_duration(pipeline, GST_FORMAT_TIME, &dur);

    static auto is_pixbuf_interesting = [](Glib::RefPtr<Gdk::Pixbuf> &p)
    {
        size_t len      = p->get_rowstride() * p->get_height();
        guint8 *buf     = p->get_pixels();
        double xbar     = 0.0,
               variance = 0.0;

        for (size_t i = 0; i < len; ++i)
            xbar += static_cast<double>(buf[i]);
        xbar /= static_cast<double>(len);

        for (size_t i = 0; i < len; ++i)
            variance += std::pow(static_cast<double>(buf[i]) - xbar, 2);

        return variance > 256.0;
    };

    // Looks at up to 6 different frames (unless duration is -1)
    // for a pixbuf that is "interesting"
    for (auto offset : { 1.0 / 3.0, 2.0 / 3.0, 0.1, 0.5, 0.9 })
    {
        pos = dur == -1 ? 1 * GST_SECOND : dur / GST_MSECOND * offset * GST_MSECOND;

        if (!gst_element_seek_simple(pipeline, GST_FORMAT_TIME,
            static_cast<GstSeekFlags>(GST_SEEK_FLAG_KEY_UNIT | GST_SEEK_FLAG_FLUSH), pos))
            break;

        g_signal_emit_by_name(sink, "pull-preroll", &sample, NULL);

        if (sample)
        {
            GstBuffer *buffer;
            GstCaps *caps;
            GstStructure *s;

            caps = gst_sample_get_caps(sample);
            if (!caps)
                break;

            s = gst_caps_get_structure(caps, 0);

            gst_structure_get_int(s, "width", &w);
            gst_structure_get_int(s, "height", &h);

            buffer = gst_sample_get_buffer(sample);
            gst_buffer_map(buffer, &map, GST_MAP_READ);
            pixbuf = Gdk::Pixbuf::create_from_data(map.data,
                                                    Gdk::COLORSPACE_RGB, false, 8, w, h,
                                                    GST_ROUND_UP_4(w * 3));

            /* save the pixbuf */
            gst_buffer_unmap (buffer, &map);

            if (dur == -1 || is_pixbuf_interesting(pixbuf))
                break;
        }
    }

    gst_object_unref(sink);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
#else
    (void)w;
    (void)h;
#endif // HAVE_GSTREAMER
    return pixbuf;
}

void Image::save_thumbnail(Glib::RefPtr<Gdk::Pixbuf> &pixbuf, const gchar *mimeType) const
{
    struct stat fileInfo;
    if (stat(m_Path.c_str(), &fileInfo) == 0)
    {
        std::vector<std::string> opts =
        {
            "tEXt::Thumb::URI",
            "tEXt::Thumb::MTime",
            "tEXt::Thumb::Size",
            "tEXt::Thumb::Image::Mimetype",
            "tEXt::Software"
        },
        vals =
        {
            Glib::filename_to_uri(m_Path),      // URI
            std::to_string(fileInfo.st_mtime),  // MTime
            std::to_string(fileInfo.st_size),   // Size
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
