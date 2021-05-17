#include "image.h"
using namespace AhoViewer::Booru;

#include "browser.h"
#include "settings.h"
#include "site.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <utility>

Image::Image(std::string path,
             std::string url,
             std::string thumb_path,
             std::string thumb_url,
             std::string post_url,
             const std::string& notes_url,
             std::vector<Tag> tags,
             PostInfo& post_info,
             std::shared_ptr<Site> site,
             ImageFetcher& fetcher)
    : AhoViewer::Image{ path },
      m_Url{ std::move(url) },
      m_ThumbnailUrl{ std::move(thumb_url) },
      m_PostUrl{ std::move(post_url) },
      m_Tags{ std::move(tags) },
      m_PostInfo{ std::move(post_info) },
      m_Site{ std::move(site) },
      m_ImageFetcher{ fetcher },
      m_LastDraw{ std::chrono::steady_clock::now() },
      m_Curler{ m_Url, m_Site->get_share_handle() },
      m_ThumbnailCurler{ m_ThumbnailUrl, m_Site->get_share_handle() },
      m_NotesCurler{ notes_url, m_Site->get_share_handle() }
{
    m_ThumbnailPath = std::move(thumb_path);

    if (!m_IsWebM)
        m_Curler.signal_write().connect(sigc::mem_fun(*this, &Image::on_write));

    m_ThumbnailCurler.signal_finished().connect([&]() { m_ThumbnailCond.notify_one(); });
    m_ThumbnailCurler.set_referer(m_Site->get_url());
    m_Curler.set_referer(m_PostUrl);
    m_Curler.signal_progress().connect(sigc::mem_fun(*this, &Image::on_progress));
    m_Curler.signal_finished().connect(sigc::mem_fun(*this, &Image::on_finished));

    if (!notes_url.empty())
    {
        m_NotesCurler.signal_finished().connect(sigc::mem_fun(*this, &Image::on_notes_downloaded));
        m_ImageFetcher.add_handle(&m_NotesCurler);
    }
}

Image::~Image()
{
    cancel_download();
    cancel_thumbnail_download();
}

// True if the data is being downloaded by the curler
// or if the pixbuf is being loaded from the saved local file
bool Image::is_loading() const
{
    return (m_IsWebM && !Glib::file_test(m_Path, Glib::FILE_TEST_EXISTS)) || m_Curler.is_active() ||
           AhoViewer::Image::is_loading();
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
            std::unique_lock<std::mutex> lock{ m_ThumbnailMutex };
            m_ThumbnailCond.wait(lock, [&]() {
                return m_ThumbnailCurler.is_cancelled() || !m_ThumbnailCurler.is_active();
            });
        }
        if (!m_ThumbnailCurler.is_cancelled() && m_ThumbnailCurler.get_response() == CURLE_OK)
        {
            m_ThumbnailCurler.save_file(m_ThumbnailPath);

            m_ThumbnailLock.lock();
            Glib::RefPtr<Gio::File> file{ Gio::File::create_for_path(m_ThumbnailPath) };

            try
            {
                m_UnscaledThumbnailPixbuf = Gdk::Pixbuf::create_from_stream(file->read(), c);
            }
            catch (...)
            {
                if (!c->is_cancelled())
                    std::cerr << "Error while loading thumbnail " << m_ThumbnailPath << " for "
                              << get_filename() << std::endl;
            }
            m_ThumbnailLock.unlock();

            // Make the actual thumbnail square by cropping/scaling the original pixbuf
            if (m_UnscaledThumbnailPixbuf && !c->is_cancelled())
            {
                auto orig_width{ m_UnscaledThumbnailPixbuf->get_width() },
                    orig_height{ m_UnscaledThumbnailPixbuf->get_height() };
                auto m{ std::min(orig_width, orig_height) };
                int x{ (orig_width - m) / 2 }, y{ (orig_height - m) / 2 };

                m_ThumbnailPixbuf =
                    Gdk::Pixbuf::create_subpixbuf(m_UnscaledThumbnailPixbuf, x, y, m, m);

                if (m != BooruThumbnailSize)
                    m_ThumbnailPixbuf = m_ThumbnailPixbuf->scale_simple(
                        BooruThumbnailSize, BooruThumbnailSize, Gdk::INTERP_BILINEAR);
            }
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
        // Load the local file
        if (Glib::file_test(m_Path, Glib::FILE_TEST_EXISTS))
        {
            AhoViewer::Image::load_pixbuf(c);
        }
        // This will either start the download and do nothing, or if the
        // download is already started and the pixbuf loader has created a
        // pixbuf set m_Pixbuf to that loader pixbuf
        else if (!start_download() && !m_IsWebM && m_Loader && m_Loader->get_pixbuf())
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

void Image::save(const std::string& path)
{
    if (!Glib::file_test(m_Path, Glib::FILE_TEST_EXISTS))
    {
        start_download();

        std::unique_lock<std::mutex> lock{ m_DownloadMutex };
        m_DownloadCond.wait(lock, [&]() {
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

        // XXX: This doesn't save the tag type, should it? I never use this
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
        if (!m_IsWebM)
        {
            m_Loader = Gdk::PixbufLoader::create();
            m_Loader->signal_area_prepared().connect(
                sigc::mem_fun(*this, &Image::on_area_prepared));
            m_Loader->signal_area_updated().connect(sigc::mem_fun(*this, &Image::on_area_updated));
        }

        m_ImageFetcher.add_handle(&m_Curler);

        return true;
    }

    return false;
}

void Image::close_loader()
{
    std::scoped_lock lock{ m_DownloadMutex };
    if (m_Loader)
    {
        try
        {
            m_Loader->close();
        }
        catch (...)
        {
        }
        m_Loader.reset();
    }
}

void Image::on_write(const unsigned char* d, size_t l)
{
    if (m_Curler.is_cancelled())
        return;

    if (!m_GIFanim && !m_IsGifChecked && m_Curler.get_data_size() >= 4)
    {
        m_IsGifChecked = true;
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
        std::scoped_lock lock{ m_DownloadMutex };
        if (!m_Loader)
            return;

        m_Loader->write(d, l);
    }
    catch (const Gdk::PixbufError& ex)
    {
        std::cerr << ex.what() << std::endl;
        cancel_download();
        m_PixbufError = true;
        m_SignalDownloadError(ex.what());
    }
}

void Image::on_progress()
{
    curl_off_t c, t;
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
            m_GIFdata     = new unsigned char[m_GIFdataSize];
            memcpy(m_GIFdata, m_Curler.get_data(), m_GIFdataSize);

            AhoViewer::Image::load_gif();
        }
        m_Curler.save_file_async(m_Path, [&](Glib::RefPtr<Gio::AsyncResult>& r) {
            try
            {
                m_Curler.save_file_finish(r);
                m_Loading = false;
                m_SignalPixbufChanged();
                m_DownloadCond.notify_one();
            }
            catch (const Gio::Error& e)
            {
                if (e.code() != Gio::Error::CANCELLED)
                    std::cerr << "Failed to save file '" << get_filename() << "'" << std::endl
                              << "  Error code: " << e.code() << std::endl;
            }
        });
    }
    else
    {
        std::cerr << "Booru::Image::on_finished: Curler received no data yet finished!"
                  << std::endl;
    }

    close_loader();
    m_UnscaledThumbnailPixbuf.reset();
}

void Image::on_area_prepared()
{
    m_ThumbnailLock.lock_shared();
    if (m_UnscaledThumbnailPixbuf && m_UnscaledThumbnailPixbuf != get_missing_pixbuf())
    {
        Glib::RefPtr<Gdk::Pixbuf> pixbuf = m_Loader->get_pixbuf();
        m_UnscaledThumbnailPixbuf->composite(
            pixbuf,
            0,
            0,
            pixbuf->get_width(),
            pixbuf->get_height(),
            0,
            0,
            static_cast<double>(pixbuf->get_width()) / m_UnscaledThumbnailPixbuf->get_width(),
            static_cast<double>(pixbuf->get_height()) / m_UnscaledThumbnailPixbuf->get_height(),
            Gdk::INTERP_BILINEAR,
            255);
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
    int ms = std::clamp((p->get_width() + p->get_height()) / 60.f, 100.f, 800.f);
    if (!m_Curler.is_cancelled() && steady_clock::now() >= m_LastDraw + milliseconds(ms))
    {
        m_SignalPixbufChanged();
        m_LastDraw = steady_clock::now();
    }
}

void Image::on_notes_downloaded()
{
    m_Notes = m_Site->parse_note_data(m_NotesCurler.get_data(), m_NotesCurler.get_data_size());

    if (m_Notes.size() > 0)
        m_SignalNotesChanged();
}
