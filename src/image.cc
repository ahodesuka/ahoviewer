#include <iostream>

#include "config.h"
#include "image.h"
#include "settings.h"
using namespace AhoViewer;

std::string Image::ThumbnailDir = Glib::build_filename(Glib::get_home_dir(), ".thumbnails", "normal");

bool Image::is_valid(const std::string &path)
{
    return gdk_pixbuf_get_file_info(path.c_str(), 0, 0) != NULL;
}

const Glib::RefPtr<Gdk::Pixbuf>& Image::get_missing_pixbuf()
{
    static const Glib::RefPtr<Gdk::Pixbuf> pixbuf =
        Gtk::Invisible().render_icon(Gtk::Stock::MISSING_IMAGE, Gtk::ICON_SIZE_DIALOG);

    return pixbuf;
}

Image::Image(const std::string &path)
  : m_Path(path),
    m_SignalPixbufChanged()
{

}

Image::Image(const std::string &path, const std::string &thumb_path)
  : m_Path(path),
    m_ThumbnailPath(thumb_path),
    m_SignalPixbufChanged()
{

}

Image::~Image()
{

}

std::string Image::get_filename() const
{
    return Glib::build_filename(
            Glib::path_get_basename(Glib::path_get_dirname(m_Path)),
            Glib::path_get_basename(m_Path));
}

const Glib::RefPtr<Gdk::Pixbuf>& Image::get_pixbuf()
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
    if (!m_Pixbuf)
    {
        Glib::RefPtr<Gdk::Pixbuf> p = Gdk::Pixbuf::create_from_file(m_Path);
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

// FIXME: https://bugzilla.gnome.org/show_bug.cgi?id=735422 workaround
Glib::RefPtr<Gdk::Pixbuf> Image::create_pixbuf_at_size(const std::string &path,
                                                       const double w, const double h)
{
    Glib::RefPtr<Gdk::Pixbuf> pixbuf = Gdk::Pixbuf::create_from_file(path);
    double ratio = std::min(w / pixbuf->get_width(), h / pixbuf->get_height());

    return pixbuf->scale_simple(pixbuf->get_width() * ratio,
                                pixbuf->get_height() * ratio, Gdk::INTERP_BILINEAR);
}

void Image::create_save_thumbnail()
{
#ifdef __linux__
    int w, h;
    GdkPixbufFormat *format = gdk_pixbuf_get_file_info(m_Path.c_str(), &w, &h);
    if (Settings.get_bool("SaveThumbnails") && format && (w > 128 || h > 128))
    {
        Glib::RefPtr<Gdk::Pixbuf> pixbuf;
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

        struct stat fileInfo;
        if (stat(m_Path.c_str(), &fileInfo) == 0)
        {
            gchar **mimeTypes = gdk_pixbuf_format_get_mime_types(format);
            std::vector<std::string> opts =
            {
                "tEXt::Thumb::URI",
                "tEXt::Thumb::MTime",
                "tEXt::Thumb::Size",
                "tEXt::Thumb::Image::Width",
                "tEXt::Thumb::Image::Height",
                "tEXt::Thumb::Image::Mimetype",
                "tEXt::Software"
            }, vals;

            vals.push_back(Glib::filename_to_uri(m_Path));
            vals.push_back(std::to_string(fileInfo.st_mtime));
            vals.push_back(std::to_string(fileInfo.st_size));
            vals.push_back(std::to_string(w));
            vals.push_back(std::to_string(h));
            vals.push_back(mimeTypes[0]);
            vals.push_back(PACKAGE_STRING);

            g_strfreev(mimeTypes);

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
#endif // __linux__
    create_thumbnail();
}
