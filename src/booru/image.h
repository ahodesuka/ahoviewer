#ifndef _BOORUIMAGE_H_
#define _BOORUIMAGE_H_

#include <condition_variable>
#include <set>
#include <shared_mutex>

#include "../image.h"
#include "curler.h"
#include "imagefetcher.h"

namespace AhoViewer
{
    namespace Booru
    {
        class Site;
        class Image : public AhoViewer::Image,
                      public sigc::trackable
        {
            friend class Browser;

            using SignalProgressType = sigc::signal<void, const Image*, double, double>;
            using SignalDownloadErrorType = sigc::signal<void, const std::string&>;
        public:
            Image(const std::string &path, const std::string &url,
                  const std::string &thumbPath, const std::string &thumbUrl,
                  const std::string &postUrl,
                  std::set<std::string> tags,
                  const std::string &notesUrl,
                  std::shared_ptr<Site> site,
                  ImageFetcher &fetcher);
            virtual ~Image() override;

            std::set<std::string> get_tags() const { return m_Tags; }

            std::string get_url() const { return m_Url; }
            std::string get_post_url() const { return m_PostUrl; }

            virtual bool is_loading() const override;
            virtual std::string get_filename() const override;
            virtual const Glib::RefPtr<Gdk::Pixbuf>& get_thumbnail(Glib::RefPtr<Gio::Cancellable>) override;

            virtual void load_pixbuf(Glib::RefPtr<Gio::Cancellable> c) override;
            virtual void reset_pixbuf() override;

            void save(const std::string &path);
            void cancel_download();
            void cancel_thumbnail_download();

            SignalProgressType signal_progress() const { return m_SignalProgress; }
            SignalDownloadErrorType signal_download_error() const { return m_SignalDownloadError; }

            static const size_t BooruThumbnailSize = 150;
        private:
            bool start_download();
            void close_loader();

            void on_write(const unsigned char *d, size_t l);
            void on_progress();
            void on_finished();
            void on_area_prepared();
            void on_area_updated(int, int, int, int);
            void on_notes_downloaded();

            std::string m_Url, m_ThumbnailUrl, m_PostUrl;
            std::set<std::string> m_Tags;
            std::shared_ptr<Site> m_Site;
            ImageFetcher &m_ImageFetcher;

            time_point_t m_LastDraw;

            Curler m_Curler,
                   m_ThumbnailCurler,
                   m_NotesCurler;
            Glib::RefPtr<Gdk::PixbufLoader> m_Loader;
            bool m_PixbufError { false },
                 m_isGIFChecked{ false };
            std::shared_mutex m_ThumbnailLock;

            std::condition_variable m_DownloadCond, m_ThumbnailCond;
            std::mutex m_DownloadMutex, m_ThumbnailMutex;

            SignalProgressType m_SignalProgress;
            SignalDownloadErrorType m_SignalDownloadError;
        };
    }
}

#endif /* _BOORUIMAGE_H_ */
