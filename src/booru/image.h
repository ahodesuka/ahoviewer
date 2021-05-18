#pragma once

#include "../image.h"
#include "../util.h"
#include "curler.h"
#include "imagefetcher.h"

#include <condition_variable>
#include <shared_mutex>
#include <vector>

namespace AhoViewer::Booru
{
    class Site;
    class Image : public AhoViewer::Image, public sigc::trackable
    {
        friend class Browser;

        using SignalProgressType      = sigc::signal<void, const Image*, double, double>;
        using SignalDownloadErrorType = sigc::signal<void, const std::string&>;

    public:
        Image(std::string path,
              std::string url,
              std::string thumb_path,
              std::string thumb_url,
              std::string post_url,
              const std::string& notes_url,
              std::vector<Tag> tags,
              PostInfo& post_info,
              std::shared_ptr<Site> site,
              ImageFetcher& fetcher);
        ~Image() override;

        std::vector<Tag>& get_tags() { return m_Tags; }

        std::string get_url() const { return m_Url; }
        std::string get_post_url() const { return m_PostUrl; }
        const PostInfo& get_post_info() const { return m_PostInfo; }

        bool is_loading() const override;
        std::string get_filename() const override;
        const Glib::RefPtr<Gdk::Pixbuf>& get_thumbnail(Glib::RefPtr<Gio::Cancellable>) override;

        void load_pixbuf(Glib::RefPtr<Gio::Cancellable> c) override;
        void reset_pixbuf() override;

        void save(const std::string& path);
        void cancel_download();
        void cancel_thumbnail_download();

        SignalProgressType signal_progress() const { return m_SignalProgress; }
        SignalDownloadErrorType signal_download_error() const { return m_SignalDownloadError; }

        static const size_t BooruThumbnailSize{ 125 };

    private:
        bool start_download();
        void close_loader();

        void on_write(const unsigned char* d, size_t l);
        void on_progress();
        void on_finished();
        void on_area_prepared();
        void on_area_updated(int, int, int, int);
        void on_notes_downloaded();

        std::string m_Url, m_ThumbnailUrl, m_PostUrl;
        std::vector<Tag> m_Tags;
        const PostInfo m_PostInfo;
        std::shared_ptr<Site> m_Site;
        ImageFetcher& m_ImageFetcher;

        time_point_t m_LastDraw;

        Curler m_Curler, m_ThumbnailCurler, m_NotesCurler;
        Glib::RefPtr<Gdk::PixbufLoader> m_Loader;
        Glib::RefPtr<Gdk::Pixbuf> m_UnscaledThumbnailPixbuf;
        bool m_PixbufError{ false }, m_IsGifChecked{ false };
        std::shared_mutex m_ThumbnailLock;

        std::condition_variable m_DownloadCond, m_ThumbnailCond;
        std::mutex m_DownloadMutex, m_ThumbnailMutex;

        SignalProgressType m_SignalProgress;
        SignalDownloadErrorType m_SignalDownloadError;
    };
}
