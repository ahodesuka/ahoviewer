#include <chrono>
#include <iostream>

#include "image.h"
using namespace AhoViewer::Booru;

#include "browser.h"

Image::Image(const std::string &path, const std::string &url,
             const std::string &thumbPath, const std::string &thumbUrl,
             const std::string &postUrl,
             std::set<std::string> tags, const Page &page)
  : AhoViewer::Image(path),
    m_Url(url),
    m_ThumbnailUrl(thumbUrl),
    m_PostUrl(postUrl),
    m_Tags(tags),
    m_Page(page),
    m_Curler(m_Url),
    m_PixbufError(false)
{
    m_ThumbnailPath = thumbPath;

    if (!m_isWebM)
        m_Curler.signal_write().connect(sigc::mem_fun(*this, &Image::on_write));

    if (m_isWebM && !Glib::file_test(m_Path, Glib::FILE_TEST_EXISTS))
        m_Loading = true;

    m_Curler.set_referer(m_PostUrl);
    m_Curler.signal_progress().connect(sigc::mem_fun(*this, &Image::on_progress));
    m_Curler.signal_finished().connect(sigc::mem_fun(*this, &Image::on_finished));
}

Image::~Image()
{
    cancel_download();
}

std::string Image::get_filename() const
{
    return Glib::build_filename(m_Page.get_site()->get_name(), Glib::path_get_basename(m_Path));
}

const Glib::RefPtr<Gdk::Pixbuf>& Image::get_thumbnail()
{
    if (!m_ThumbnailPixbuf)
    {
        Curler curler(m_ThumbnailUrl);

        m_ThumbnailLock.writer_lock();
        if (curler.perform())
        {
            curler.save_file(m_ThumbnailPath);

            try
            {
                m_ThumbnailPixbuf = create_pixbuf_at_size(m_ThumbnailPath, 128, 128);
            }
            catch (const Gdk::PixbufError &ex)
            {
                std::cerr << ex.what() << std::endl;
                m_ThumbnailPixbuf = get_missing_pixbuf();
            }
        }
        else
        {
            std::cerr << "Error while downloading thumbnail " << m_ThumbnailUrl
                      << " " << std::endl << "  " << curler.get_error() << std::endl;
            m_ThumbnailPixbuf = get_missing_pixbuf();
        }
        m_ThumbnailLock.writer_unlock();
    }

    return m_ThumbnailPixbuf;
}

void Image::load_pixbuf()
{
    if (!m_Pixbuf && !m_PixbufError)
    {
        if (Glib::file_test(m_Path, Glib::FILE_TEST_EXISTS))
        {
            AhoViewer::Image::load_pixbuf();
        }
        else if (!start_download() && !m_isWebM && m_Loader->get_animation())
        {
            m_Pixbuf = m_Loader->get_animation();
        }
    }
}

void Image::reset_pixbuf()
{
    if (m_Loading)
        cancel_download();

    AhoViewer::Image::reset_pixbuf();
}

void Image::save(const std::string &path)
{
    if (!Glib::file_test(m_Path, Glib::FILE_TEST_EXISTS))
    {
        start_download();

        Glib::Threads::Mutex::Lock lock(m_DownloadMutex);
        while (!m_Curler.is_cancelled() && !Glib::file_test(m_Path, Glib::FILE_TEST_EXISTS))
            m_DownloadCond.wait(m_DownloadMutex);
    }

    if (m_Curler.is_cancelled())
        return;

    Glib::RefPtr<Gio::File> src = Gio::File::create_for_path(m_Path),
                            dst = Gio::File::create_for_path(path);
    src->copy(dst, Gio::FILE_COPY_OVERWRITE);
}

void Image::cancel_download()
{
    Glib::Threads::Mutex::Lock lock(m_DownloadMutex);
    m_Curler.cancel();
    m_Curler.clear();

    if (m_Loader)
    {
        try { m_Loader->close(); }
        catch (...) { }
        m_Loader.reset();
    }

    m_DownloadCond.signal();
}

/**
 * Returns true if the download was started
 **/
bool Image::start_download()
{
    if (!m_Curler.is_active())
    {
        m_Page.get_image_fetcher().add_handle(&m_Curler);
        m_Loading = true;

        if (!m_isWebM)
        {
            m_Loader = Gdk::PixbufLoader::create();
            m_Loader->signal_area_prepared().connect(sigc::mem_fun(*this, &Image::on_area_prepared));
            m_Loader->signal_area_updated().connect(sigc::mem_fun(*this, &Image::on_area_updated));
        }

        return true;
    }

    return false;
}

void Image::on_write(const unsigned char *d, size_t l)
{
    try
    {
        Glib::Threads::Mutex::Lock lock(m_DownloadMutex);
        m_Loader->write(d, l);
    }
    catch (const Gdk::PixbufError &ex)
    {
        std::cerr << ex.what() << std::endl;
        cancel_download();
        m_PixbufError = true;
    }
}

void Image::on_progress()
{
    double c, t;
    m_Curler.get_progress(c, t);
    m_SignalProgress(c, t);
}

void Image::on_finished()
{
    Glib::Threads::Mutex::Lock lock(m_DownloadMutex);

    m_Curler.save_file(m_Path);
    m_Curler.clear();

    if (m_Loader)
    {
        m_Loader->close();
        m_Loader.reset();
    }

    m_Loading = false;

    m_SignalPixbufChanged();
    m_DownloadCond.signal();
}

void Image::on_area_prepared()
{
    m_ThumbnailLock.reader_lock();
    if (m_ThumbnailPixbuf && m_ThumbnailPixbuf != get_missing_pixbuf())
    {
        Glib::RefPtr<Gdk::Pixbuf> pixbuf = m_Loader->get_pixbuf();
        m_ThumbnailPixbuf->composite(pixbuf, 0, 0, pixbuf->get_width(), pixbuf->get_height(), 0, 0,
                                     static_cast<double>(pixbuf->get_width()) / m_ThumbnailPixbuf->get_width(),
                                     static_cast<double>(pixbuf->get_height()) / m_ThumbnailPixbuf->get_height(),
                                     Gdk::INTERP_BILINEAR, 255);
    }
    m_ThumbnailLock.reader_unlock();

    m_Pixbuf = m_Loader->get_animation();

    if (!m_Curler.is_cancelled())
        m_SignalPixbufChanged();
}

void Image::on_area_updated(int, int, int, int)
{
    using namespace std::chrono;
    if (!m_Curler.is_cancelled() && steady_clock::now() >= m_LastDraw + milliseconds(100))
    {
        m_SignalPixbufChanged();
        m_LastDraw = steady_clock::now();
    }
}
