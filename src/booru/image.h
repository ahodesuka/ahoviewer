#ifndef _BOORUIMAGE_H_
#define _BOORUIMAGE_H_

#include "../image.h"
#include "page.h"

namespace AhoViewer
{
    namespace Booru
    {
        class Image : public AhoViewer::Image
        {
        public:
            Image(const std::string &path, const std::string &url,
                  const std::string &thumbPath, const std::string &thumbUrl,
                  std::vector<std::string> tags, Page *page);
            virtual ~Image();

            Curler* get_curler() const { return m_Curler; }
            std::vector<std::string> get_tags() const { return m_Tags; }
        protected:
            virtual std::string get_filename() const;
            virtual const Glib::RefPtr<Gdk::Pixbuf>& get_thumbnail();

            virtual void load_pixbuf();
        private:
            void on_area_prepared();
            void on_area_updated(int, int, int, int);

            std::string m_Url, m_ThumbnailUrl;
            std::vector<std::string> m_Tags;
            Curler *m_Curler;
            Glib::RefPtr<Gdk::PixbufLoader> m_Loader;
            Page *m_Page;
        };
    }
}

#endif /* _BOORUIMAGE_H_ */
