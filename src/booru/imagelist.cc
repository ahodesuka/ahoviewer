#include "imagelist.h"
using namespace AhoViewer::Booru;

#include "image.h"
#include "page.h"
#include "tempdir.h"

ImageList::ImageList(Widget *w)
  : AhoViewer::ImageList(w),
    m_Size(0)
{

}

ImageList::~ImageList()
{
    clear();
}

void ImageList::clear()
{
    AhoViewer::ImageList::clear();
    if (!m_Path.empty()) {
        TempDir::get_instance().remove_dir(m_Path);
        m_Path.clear();
    }
    m_Size = 0;
}

std::string ImageList::get_path()
{
    static int id = 1;

    if (m_Path.empty())
    {
        m_Path = TempDir::get_instance().make_dir(std::to_string(id++));
        g_mkdir_with_parents(Glib::build_filename(m_Path, "thumbnails").c_str(), 0755);
    }

    return m_Path;
}

void ImageList::load(const xmlDocument &posts, const Page &page)
{
    std::string c = posts.get_attribute("count");
    if (!c.empty())
       m_Size = std::stoul(c);

    for (const xmlDocument::Node &post : posts.get_children())
    {
        std::string thumbUrl  = post.get_attribute("preview_url"),
                    thumbPath = Glib::build_filename(get_path(), "thumbnails",
                                                   Glib::uri_unescape_string(Glib::path_get_basename(thumbUrl))),
                    imageUrl  = post.get_attribute("file_url"),
                    imagePath = Glib::build_filename(get_path(),
                                                   Glib::uri_unescape_string(Glib::path_get_basename(imageUrl)));

        std::istringstream ss(post.get_attribute("tags"));
        std::set<std::string> tags { std::istream_iterator<std::string>(ss),
                                     std::istream_iterator<std::string>() };
        page.get_site()->add_tags(tags);

        if (thumbUrl[0] == '/')
        {
            if (thumbUrl[1] == '/')
                thumbUrl = "https:" + thumbUrl;
            else
                thumbUrl = page.get_site()->get_url() + thumbUrl;
        }

        if (imageUrl[0] == '/')
        {
            if (imageUrl[1] == '/')
                imageUrl = "https:" + imageUrl;
            else
                imageUrl = page.get_site()->get_url() + imageUrl;
        }

        std::string postUrl = page.get_site()->get_post_url(post.get_attribute("id"));

        m_Images.push_back(std::make_shared<Booru::Image>(imagePath, imageUrl, thumbPath, thumbUrl, postUrl, tags, page));
    }

    // If thumbnails are still loading from the last page
    // the operation needs to be cancelled, all the
    // thumbnails will be loaded in the new thread
    if (m_ThumbnailThread.joinable())
    {
        m_ThumbnailCancel->cancel();
        m_ThumbnailThread.join();
    }

    m_ThumbnailThread = std::thread(sigc::mem_fun(*this, &ImageList::load_thumbnails));

    // only call set_current if this is the first page
    if (page.get_page_num() == 1)
        set_current(m_Index, false, true);
    else
        m_SignalChanged(m_Images[m_Index]);
}

void ImageList::set_current(const size_t index, const bool fromWidget, const bool force)
{
    if (index == m_Index && !force)
        return;

    m_Index = index;
    m_SignalChanged(m_Images[m_Index]);
    update_cache();

    if (!fromWidget)
        m_Widget->set_selected(m_Index);
}
