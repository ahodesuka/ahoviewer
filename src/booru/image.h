#ifndef _BOORUIMAGE_H_
#define _BOORUIMAGE_H_

#include "../image.h"
#include "curler.h"
#include "page.h"

namespace AhoViewer
{
    namespace Booru
    {
        class Image : public AhoViewer::Image,
                      public sigc::trackable
        {
            typedef sigc::signal<void, double, double> SignalProgressType;
        public:
            Image(const std::string &path, const std::string &url,
                  const std::string &thumbPath, const std::string &thumbUrl,
                  const std::string &postUrl,
                  std::set<std::string> tags, const Page &page);
            virtual ~Image();

            time_point_t get_start_time() const { return m_Curler.get_start_time(); }
            std::set<std::string> get_tags() const { return m_Tags; }

            std::string get_url() const { return m_Url; }
            std::string get_post_url() const { return m_PostUrl; }

            virtual std::string get_filename() const;
            virtual const Glib::RefPtr<Gdk::Pixbuf>& get_thumbnail();

            virtual void load_pixbuf();

            void save(const std::string &path);
            void cancel_download();

            SignalProgressType signal_progress() const { return m_SignalProgress; }

            static const size_t BooruThumbnailSize = 150;
        private:
            bool start_download();

            void on_write(const unsigned char *d, size_t l);
            void on_progress();
            void on_finished();
            void on_area_prepared();
            void on_area_updated(int, int, int, int);

            std::string m_Url, m_ThumbnailUrl, m_PostUrl;
            std::set<std::string> m_Tags;
            const Page &m_Page;

            time_point_t m_LastDraw;

            Curler m_Curler, m_ThumbnailCurler;
            Glib::RefPtr<Gdk::PixbufLoader> m_Loader;
            bool m_PixbufError;
            Glib::Threads::RWLock m_ThumbnailLock;

            Glib::Threads::Cond m_DownloadCond;
            Glib::Threads::Mutex m_DownloadMutex;

            SignalProgressType m_SignalProgress;
        };
    }
}

#endif /* _BOORUIMAGE_H_ */
