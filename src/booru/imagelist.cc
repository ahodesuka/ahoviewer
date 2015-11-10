#include "imagelist.h"
using namespace AhoViewer::Booru;

#include "image.h"
#include "page.h"

ImageList::ImageList(Widget *w)
  : AhoViewer::ImageList(w),
    m_Size(0)
{

}

void ImageList::clear()
{
    AhoViewer::ImageList::clear();
    m_Size = 0;
}

void ImageList::load(const xmlDocument &posts, const Page &page)
{
    std::string c = posts.get_attribute("count");
    if (!c.empty())
       m_Size = std::stoul(c);

    for (const xmlDocument::Node &post : posts.get_children())
    {
        std::string thumbUrl  = post.get_attribute("preview_url"),
                    thumbPath = Glib::build_filename(page.get_site()->get_path(), "thumbnails",
                                                   Glib::uri_unescape_string(Glib::path_get_basename(thumbUrl))),
                    imageUrl  = post.get_attribute("file_url"),
                    imagePath = Glib::build_filename(page.get_site()->get_path(),
                                                   Glib::uri_unescape_string(Glib::path_get_basename(imageUrl)));

        std::istringstream ss(post.get_attribute("tags"));
        std::set<std::string> tags { std::istream_iterator<std::string>(ss),
                                     std::istream_iterator<std::string>() };
        page.get_site()->add_tags(tags);

        if (thumbUrl[0] == '/')
            thumbUrl = page.get_site()->get_url() + thumbUrl;

        if (imageUrl[0] == '/')
            imageUrl = page.get_site()->get_url() + imageUrl;

        std::string postUrl = page.get_site()->get_post_url(post.get_attribute("id"));

        m_Images.push_back(std::make_shared<Booru::Image>(imagePath, imageUrl, thumbPath, thumbUrl, postUrl, tags, page));
    }

    // If thumbnails are still loading from the last page
    // the operation needs to be cancelled, all the
    // thumbnails will be loaded in the new thread
    if (m_ThumbnailThread)
    {
        m_ThumbnailCancel->cancel();
        m_ThumbnailThread->join();
    }

    m_ThumbnailThread = Glib::Threads::Thread::create(sigc::mem_fun(*this, &ImageList::load_thumbnails));

    // only call set_current if this is the first page
    if (page.get_page_num() == 1)
        set_current(m_Index, false, true);
    else
        m_SignalChanged(m_Images[m_Index]);
}
