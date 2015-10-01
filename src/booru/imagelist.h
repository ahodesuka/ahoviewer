#ifndef _BOORUIMAGELIST_H_
#define _BOORUIMAGELIST_H_

#include "../imagelist.h"

namespace AhoViewer
{
    namespace Booru
    {
        class Page;
        class ImageList : public AhoViewer::ImageList
        {
        public:
            ImageList(Widget *w);

            virtual size_t get_size() const { return m_Size ? m_Size : AhoViewer::ImageList::get_size(); }
            size_t get_vector_size() const { return m_Images.size(); }

            virtual void clear();
            void load(const std::shared_ptr<xmlDocument> posts, Booru::Page *const page);
        private:
            size_t m_Size;
        };
    }
}

#endif /* _BOORUIMAGELIST_H_ */
