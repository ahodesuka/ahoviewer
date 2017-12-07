#ifndef _BOORUIMAGELIST_H_
#define _BOORUIMAGELIST_H_

#include "../imagelist.h"

namespace AhoViewer
{
    namespace Booru
    {
        class ImageFetcher;
        class Page;
        class Site;
        class ImageList : public AhoViewer::ImageList
        {
        public:
            ImageList(Widget *w);
            virtual ~ImageList() override;

            std::string get_path();
            virtual size_t get_size() const override { return m_Size ? m_Size : AhoViewer::ImageList::get_size(); }
            size_t get_vector_size() const { return m_Images.size(); }

            virtual void clear() override;
            void load(const xmlDocument &posts, const Page &page);
        protected:
            virtual void set_current(const size_t index,
                                     const bool fromWidget = false, const bool force = false) override;
            virtual void cancel_thumbnail_thread() override;
        private:
            std::unique_ptr<ImageFetcher> m_ImageFetcher;
            std::shared_ptr<Site> m_Site;

            std::string m_Path;
            size_t m_Size;
        };
    }
}

#endif /* _BOORUIMAGELIST_H_ */
