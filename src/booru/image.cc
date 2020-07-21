#include <chrono>
#include <fstream>
#include <iostream>

#include "image.h"
using namespace AhoViewer::Booru;

#include "browser.h"
#include "settings.h"
#include "site.h"

Image::Image(const std::string &path, const std::string &url,
             const std::string &thumbPath, const std::string &thumbUrl,
             const std::string &postUrl,
             std::set<std::string> tags,
             std::shared_ptr<Site> site,
             ImageFetcher &fetcher)
  : AhoViewer::Image(path),
    m_Url(url),
    m_ThumbnailUrl(thumbUrl),
    m_PostUrl(postUrl),
    m_Tags(tags),
    m_Site(site),
    m_ImageFetcher(fetcher),
    m_LastDraw(std::chrono::steady_clock::now()),
    m_Curler(m_Url, m_Site->get_share_handle()),
    m_ThumbnailCurler(m_ThumbnailUrl, m_Site->get_share_handle()),
    m_PixbufError(false),
    m_isGIFChecked(false)
{
    m_ThumbnailPath = thumbPath;

    if (!m_isWebM)
        m_Curler.signal_write().connect(sigc::mem_fun(*this, &Image::on_write));

    m_ThumbnailCurler.signal_finished().connect([&]() { m_ThumbnailCond.notify_one(); });
    m_Curler.set_referer(m_PostUrl);
    m_Curler.signal_progress().connect(sigc::mem_fun(*this, &Image::on_progress));
    m_Curler.signal_finished().connect(sigc::mem_fun(*this, &Image::on_finished));
}

Image::~Image()
{
    cancel_download();
    cancel_thumbnail_download();
}

bool Image::is_loading() const
{
    return (m_isWebM && !Glib::file_test(m_Path, Glib::FILE_TEST_EXISTS))
        || m_Curler.is_active() || (m_Loading && !m_isWebM);
}

std::string Image::get_filename() const
{
    return m_Site->get_name() + "/" + Glib::path_get_basename(m_Path);
}

const Glib::RefPtr<Gdk::Pixbuf>& Image::get_thumbnail(Glib::RefPtr<Gio::Cancellable> c)
{
    if (!m_ThumbnailPixbuf && !c->is_cancelled())
    {
        m_ImageFetcher.add_handle(&m_ThumbnailCurler);

        {
            std::unique_lock<std::mutex> lock(m_ThumbnailMutex);
            m_ThumbnailCond.wait(lock, [&]()
            {
                return m_ThumbnailCurler.is_cancelled()
                    || !m_ThumbnailCurler.is_active();
            });
        }
        if (!m_ThumbnailCurler.is_cancelled() && m_ThumbnailCurler.get_response() == CURLE_OK)
        {
            m_ThumbnailCurler.save_file(m_ThumbnailPath);

            m_ThumbnailLock.lock();
            // This doesn't need to be cancellable since booru
            // thumbnails are already small in size
            m_ThumbnailPixbuf = create_pixbuf_at_size(m_ThumbnailPath, 128, 128, Gio::Cancellable::create());
            m_ThumbnailLock.unlock();
        }
        else if (!m_ThumbnailCurler.is_cancelled())
        {
            std::cerr << "Error while downloading thumbnail " << m_ThumbnailUrl << std::endl
                      << "  " << m_ThumbnailCurler.get_error() << std::endl;
        }

        m_ThumbnailCurler.clear();
    }

    return m_ThumbnailPixbuf;
}

void Image::load_pixbuf(Glib::RefPtr<Gio::Cancellable> c)
{
    if (!m_Pixbuf && !m_PixbufError)
    {
        if (Glib::file_test(m_Path, Glib::FILE_TEST_EXISTS))
        {
            AhoViewer::Image::load_pixbuf(c);
        }
        else if (!start_download() && !m_isWebM && m_Loader && m_Loader->get_pixbuf())
        {
            m_Pixbuf = m_Loader->get_pixbuf();
        }
    }
}

void Image::reset_pixbuf()
{
    if (m_Curler.is_active())
        cancel_download();

    AhoViewer::Image::reset_pixbuf();
}

void Image::save(const std::string &path)
{
    if (!Glib::file_test(m_Path, Glib::FILE_TEST_EXISTS))
    {
        start_download();

        std::unique_lock<std::mutex> lock(m_DownloadMutex);
        m_DownloadCond.wait(lock, [&]()
        {
            return m_Curler.is_cancelled() || Glib::file_test(m_Path, Glib::FILE_TEST_EXISTS);
        });
    }

    if (m_Curler.is_cancelled())
        return;

    Glib::RefPtr<Gio::File> src = Gio::File::create_for_path(m_Path),
                            dst = Gio::File::create_for_path(path);
    src->copy(dst, Gio::FILE_COPY_OVERWRITE);

    if (Settings.get_bool("SaveImageTags"))
    {
        std::ofstream ofs(path + ".txt");

        if (ofs)
            std::copy(m_Tags.begin(), m_Tags.end(), std::ostream_iterator<std::string>(ofs, "\n"));
    }
}

void Image::cancel_download()
{
    m_Curler.cancel();
    close_loader();

    m_DownloadCond.notify_one();
}

void Image::cancel_thumbnail_download()
{
    m_ThumbnailCurler.cancel();
    m_ThumbnailCond.notify_one();
}

// Returns true if the download was started
bool Image::start_download()
{
    if (!m_Curler.is_active())
    {
        m_ImageFetcher.add_handle(&m_Curler);

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

void Image::close_loader()
{
    std::lock_guard<std::mutex> lock(m_DownloadMutex);
    if (m_Loader)
    {
        try { m_Loader->close(); }
        catch (...) { }
        m_Loader.reset();
    }
}

void Image::on_write(const unsigned char *d, size_t l)
{
    if (m_Curler.is_cancelled())
        return;

    if (!m_GIFanim && !m_isGIFChecked && m_Curler.get_data_size() >= 4)
    {
        m_isGIFChecked = true;
        if (is_gif(m_Curler.get_data()))
        {
            m_GIFanim = new gif_animation;
            gif_create(m_GIFanim, &m_BitmapCallbacks);
            m_Pixbuf.reset();
            close_loader();
        }
    }

    try
    {
        std::lock_guard<std::mutex> lock(m_DownloadMutex);
        if (!m_Loader)
            return;

        m_Loader->write(d, l);
    }
    catch (const Gdk::PixbufError &ex)
    {
        std::cerr << ex.what() << std::endl;
        cancel_download();
        m_PixbufError = true;
        m_SignalDownloadError(ex.what());
    }
}

void Image::on_progress()
{
    double c, t;
    m_Curler.get_progress(c, t);
    m_SignalProgress(this, c, t);
}

// This is called from the ImageFetcher when the doawnload finished and was not cancelled
void Image::on_finished()
{
    if (m_Curler.get_data_size() > 0)
    {
        if (m_GIFanim)
        {
            m_GIFdataSize = m_Curler.get_data_size();
            m_GIFdata = new unsigned char[m_GIFdataSize];
            memcpy(m_GIFdata, m_Curler.get_data(), m_GIFdataSize);

            AhoViewer::Image::load_gif();
        }
        m_Curler.save_file(m_Path);
        m_Curler.clear();
    }
    else
    {
        std::cerr << "Booru::Image::on_finished: Curler received no data yet finished!" << std::endl;
    }

    close_loader();

    m_Loading = false;
    m_SignalPixbufChanged();
    m_DownloadCond.notify_one();
}

void Image::on_area_prepared()
{
    m_ThumbnailLock.lock_shared();
    if (m_ThumbnailPixbuf && m_ThumbnailPixbuf != get_missing_pixbuf())
    {
        Glib::RefPtr<Gdk::Pixbuf> pixbuf = m_Loader->get_pixbuf();
        m_ThumbnailPixbuf->composite(pixbuf, 0, 0, pixbuf->get_width(), pixbuf->get_height(), 0, 0,
                                     static_cast<double>(pixbuf->get_width()) / m_ThumbnailPixbuf->get_width(),
                                     static_cast<double>(pixbuf->get_height()) / m_ThumbnailPixbuf->get_height(),
                                     Gdk::INTERP_BILINEAR, 255);
    }
    m_ThumbnailLock.unlock_shared();

    if (!m_Curler.is_cancelled())
    {
        m_Pixbuf = m_Loader->get_pixbuf();
        m_SignalPixbufChanged();
    }
}

void Image::on_area_updated(int, int, int, int)
{
    using namespace std::chrono;
    // Wait longer between draw requests for larger images
    // A better solution might be to have a drawn signal or flag and check that
    // but knowing for sure when gtk draws something seems impossible
    Glib::RefPtr<Gdk::Pixbuf> p = m_Loader->get_pixbuf();
    int ms = std::min(std::max((p->get_width() + p->get_height()) / 60.f, 100.f), 800.f);
    if (!m_Curler.is_cancelled() && steady_clock::now() >= m_LastDraw + milliseconds(ms))
    {
        m_SignalPixbufChanged();
        m_LastDraw = steady_clock::now();
    }
}
