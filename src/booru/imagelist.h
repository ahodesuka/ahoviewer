#pragma once

#include "../imagelist.h"
#include "xml.h"

namespace AhoViewer::Booru
{
    class ImageFetcher;
    class Page;
    class Site;
    class ImageList : public AhoViewer::ImageList
    {
    public:
        ImageList(Widget* w);
        ~ImageList() override;

        std::string get_path();
        size_t get_size() const override
        {
            return m_Size ? m_Size : AhoViewer::ImageList::get_size();
        }
        size_t get_vector_size() const { return m_Images.size(); }

        void clear() override;
        void load(const std::vector<PostDataTuple>& posts, const size_t posts_count = 0);
        bool is_loading() const { return m_ThreadPool.active(); }

    protected:
        void set_current(const size_t index,
                         const bool from_widget = false,
                         const bool force       = false) override;
        void cancel_thumbnail_thread() override;
        void on_thumbnail_loaded() override;

    private:
        std::unique_ptr<ImageFetcher> m_ImageFetcher;

        std::string m_Path;
        // This is the total number of posts for the given booru query
        size_t m_Size{ 0 };
    };
}
