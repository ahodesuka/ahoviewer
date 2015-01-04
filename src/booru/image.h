#ifndef _BOORUIMAGE_H_
#define _BOORUIMAGE_H_

#include "../image.h"
#include "page.h"

namespace AhoViewer
{
    namespace Booru
    {
        class Image : public AhoViewer::Image,
                      public sigc::trackable
        {
        public:
            typedef sigc::signal<void, double, double> SignalProgressType;

            Image(const std::string &path, const std::string &url,
                  const std::string &thumbPath, const std::string &thumbUrl,
                  std::set<std::string> tags, Page *page);
            virtual ~Image();

            Curler* get_curler() const { return m_Curler; }
            std::set<std::string> get_tags() const { return m_Tags; }

            SignalProgressType signal_progress() const { return m_SignalProgress; }
        protected:
            virtual std::string get_filename() const;
            virtual const Glib::RefPtr<Gdk::Pixbuf>& get_thumbnail();

            virtual void load_pixbuf();
        private:
            void on_progress();
            void on_finished();
            void on_area_prepared();
            void on_area_updated(int, int, int, int);

            std::string m_Url, m_ThumbnailUrl;
            std::set<std::string> m_Tags;
            Page *m_Page;

            double m_DownloadCurrent,
                   m_DownloadTotal;

            Curler *m_Curler;
            Glib::RefPtr<Gdk::PixbufLoader> m_Loader;
            Glib::Threads::RWLock m_ThumbnailLock;

            SignalProgressType m_SignalProgress;
        };
    }
}

#endif /* _BOORUIMAGE_H_ */
