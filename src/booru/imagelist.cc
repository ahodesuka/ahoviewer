#include "imagelist.h"
using namespace AhoViewer::Booru;

#include "image.h"
#include "imagefetcher.h"
#include "page.h"
#include "site.h"
#include "tempdir.h"

ImageList::ImageList(Widget *w)
  : AhoViewer::ImageList(w),
    m_Size(0)
{

}

ImageList::~ImageList()
{
    // Explicitly clear all signal handlers since we are destroying everything
    m_SignalCleared.clear();
    clear();
}

// This is also used when reusing the same page with a new query
void ImageList::clear()
{
    cancel_thumbnail_thread();

    // This prepares the imagefetcher for a clean death
    if (m_ImageFetcher)
        m_ImageFetcher->shutdown();

    // Clears the image vector and widget (Booru::Page)
    AhoViewer::ImageList::clear();

    if (!m_Path.empty())
    {
        TempDir::get_instance().remove_dir(m_Path);
        m_Path.clear();
    }

    m_Size = 0;
    m_ImageFetcher = nullptr;
}

std::string ImageList::get_path()
{
    if (m_Path.empty())
    {
        m_Path = TempDir::get_instance().make_dir();
        g_mkdir_with_parents(Glib::build_filename(m_Path, "thumbnails").c_str(), 0755);
    }

    return m_Path;
}

void ImageList::load(const xml::Document &posts, const Page &page)
{
    m_Site = page.get_site();

    if (!m_ImageFetcher)
        m_ImageFetcher = std::make_unique<ImageFetcher>(m_Site->get_max_connections());

    std::string c = posts.get_attribute("count");
    if (!c.empty())
       m_Size = std::stoul(c);

    for (const xml::Node &post : posts.get_children())
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
        m_Site->add_tags(tags);

        if (thumbUrl[0] == '/')
        {
            if (thumbUrl[1] == '/')
                thumbUrl = "https:" + thumbUrl;
            else
                thumbUrl = m_Site->get_url() + thumbUrl;
        }

        if (imageUrl[0] == '/')
        {
            if (imageUrl[1] == '/')
                imageUrl = "https:" + imageUrl;
            else
                imageUrl = m_Site->get_url() + imageUrl;
        }

        std::string postUrl = m_Site->get_post_url(post.get_attribute("id"));

        if (Image::is_valid_extension(imageUrl))
            m_Images.emplace_back(std::make_shared<Image>(imagePath, imageUrl,
                                                          thumbPath, thumbUrl,
                                                          postUrl, tags,
                                                          m_Site, *m_ImageFetcher));
        else
            --m_Size;
    }

    if (m_Images.empty())
        return;

    if (m_ThumbnailThread.joinable())
        m_ThumbnailThread.join();

    m_ThumbnailThread = std::thread(sigc::mem_fun(*this, &ImageList::load_thumbnails));

    // only call set_current if this is the first page
    if (page.get_page_num() == 1)
        set_current(m_Index, false, true);
    else
        m_SignalChanged(m_Images[m_Index]);
}

// Override this so we dont cancel and restart the thumbnail thread
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

// Cancel all image thumbnail curlers.
void ImageList::cancel_thumbnail_thread()
{
    m_ThumbnailCancel->cancel();

    for (auto img : *this)
    {
        auto bimage = std::static_pointer_cast<Image>(img);
        bimage->cancel_thumbnail_download();
    }
}
