#include "imagelist.h"
using namespace AhoViewer::Booru;

#include "image.h"
#include "imagefetcher.h"
#include "page.h"
#include "site.h"
#include "tempdir.h"

#include <chrono>

ImageList::ImageList(Widget* w) : AhoViewer::ImageList(w) { }

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

    m_Size             = 0;
    m_ThumbnailsLoaded = 0;
    m_ImageFetcher     = nullptr;
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

void ImageList::load(const std::vector<PostDataTuple>& posts, const size_t posts_count)
{
    auto page{ static_cast<Page*>(m_Widget) };

    if (!m_ImageFetcher)
        m_ImageFetcher = std::make_unique<ImageFetcher>(page->get_site()->get_multiplexing());

    auto old_size{ m_Images.size() };
    m_Size = posts_count;

    for (const auto& post : posts)
    {
        auto [image_url, thumb_url, post_url, notes_url, tags, post_info]{ post };
        auto junk_trimmed_image_url{ image_url }, junk_trimmed_thumb_url{ thumb_url };

        // Some image/thumb urls may have uri parameters, trim them so is_valid_extension works
        if (auto last_quest = image_url.find_last_of('?'); last_quest != std::string::npos)
            junk_trimmed_image_url = image_url.substr(0, last_quest);
        if (auto last_quest = thumb_url.find_last_of('?'); last_quest != std::string::npos)
            junk_trimmed_thumb_url = thumb_url.substr(0, last_quest);

        // Check this before we do tag stuff since it would waste time
        if (!Image::is_valid_extension(junk_trimmed_image_url))
        {
            if (m_Size != 0)
                --m_Size;
            continue;
        }

        auto thumb_path{ Glib::build_filename(
            get_path(),
            "thumbnails",
            Glib::uri_unescape_string(Glib::path_get_basename(junk_trimmed_thumb_url))) };
        auto image_path{ Glib::build_filename(
            get_path(),
            Glib::uri_unescape_string(Glib::path_get_basename(junk_trimmed_image_url))) };

        m_Images.push_back(std::make_shared<Image>(image_path,
                                                   image_url,
                                                   thumb_path,
                                                   thumb_url,
                                                   post_url,
                                                   notes_url,
                                                   tags,
                                                   post_info,
                                                   page->get_site(),
                                                   *m_ImageFetcher));
    }

    if (m_Images.empty())
        return;

    m_Widget->reserve(m_Images.size() - old_size);

    if (m_ThumbnailThread.joinable())
        m_ThumbnailThread.join();

    m_ThumbnailThread = std::thread(sigc::mem_fun(*this, &ImageList::load_thumbnails));

    // Select the first image on initial load
    if (page->get_page_num() == 1)
    {
        set_current(m_Index, false, true);
    }
    else
    {
        m_SignalChanged(m_Images[m_Index]);
    }
}

// Override this so we dont cancel and restart the thumbnail thread
void ImageList::set_current(const size_t index, const bool from_widget, const bool force)
{
    if (index == m_Index && !force)
        return;

    m_Index = index;
    m_SignalChanged(m_Images[m_Index]);
    update_cache();

    if (!from_widget)
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

void ImageList::on_thumbnail_loaded()
{
    m_SignalLoadProgress(m_ThumbnailsLoaded, m_ThumbnailsLoading);
    AhoViewer::ImageList::on_thumbnail_loaded();
}