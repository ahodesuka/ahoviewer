#include "imagelist.h"
using namespace AhoViewer::Booru;

#include "image.h"
#include "imagefetcher.h"
#include "page.h"
#include "site.h"
#include "tempdir.h"

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

    m_Size         = 0;
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

void ImageList::load(const xml::Document& posts, const Page& page)
{
    m_Site = page.get_site();

    if (!m_ImageFetcher)
        m_ImageFetcher = std::make_unique<ImageFetcher>(m_Site->get_max_connections());

    std::string c = posts.get_attribute("count");
    if (!c.empty())
        m_Size = std::stoul(c);

    for (const xml::Node& post : posts.get_children())
    {
        auto site_type{ m_Site->get_type() };
        std::string id, thumb_url, image_url, thumb_path, image_path, tags_string, notes_url,
            post_url;

        if (site_type == Type::DANBOORU_V2)
        {
            id          = post.get_value("id");
            thumb_url   = post.get_value("preview-file-url");
            image_url   = post.get_value(m_Site->use_samples() ? "large-file-url" : "file-url");
            tags_string = post.get_value("tag-string");
            // TODO: use tag category values (color tags based on category in
            // the tagview)  Issue #100
            // Category nodes are: tag-string-general, tag-string-character,
            // tag-string-copyright, tag-string-artist, tag-string-meta
        }
        else
        {
            id          = post.get_attribute("id");
            thumb_url   = post.get_attribute("preview_url");
            image_url   = post.get_attribute(m_Site->use_samples() ? "sample_url" : "file_url");
            tags_string = post.get_attribute("tags");
        }

        thumb_path =
            Glib::build_filename(get_path(),
                                 "thumbnails",
                                 Glib::uri_unescape_string(Glib::path_get_basename(thumb_url)));
        image_path = Glib::build_filename(
            get_path(), Glib::uri_unescape_string(Glib::path_get_basename(image_url)));

        std::istringstream ss{ tags_string };
        std::set<std::string> tags{ std::istream_iterator<std::string>{ ss },
                                    std::istream_iterator<std::string>{} };
        m_Site->add_tags(tags);

        if (thumb_url[0] == '/')
        {
            if (thumb_url[1] == '/')
                thumb_url = "https:" + thumb_url;
            else
                thumb_url = m_Site->get_url() + thumb_url;
        }

        if (image_url[0] == '/')
        {
            if (image_url[1] == '/')
                image_url = "https:" + image_url;
            else
                image_url = m_Site->get_url() + image_url;
        }

        bool has_notes{ false };
        // Moebooru doesnt have a has_notes attribute, instead they have
        // last_noted_at which is a unix timestamp or 0 if no notes
        if (site_type == Type::MOEBOORU)
            has_notes = post.get_attribute("last_noted_at") != "0";
        else if (site_type == Type::DANBOORU_V2)
            has_notes = post.get_value("last-noted-at") != "";
        else
            has_notes = post.get_attribute("has_notes") == "true";

        if (has_notes)
            notes_url = m_Site->get_notes_url(id);

        // safebooru.org provides the wrong file extension for thumbnails
        // All their thumbnails are .jpg, but their api gives links to with the
        // same exntension as the original images exnteion
        if (thumb_url.find("safebooru.org") != std::string::npos)
            thumb_url = thumb_url.substr(0, thumb_url.find_last_of('.')) + ".jpg";

        post_url = m_Site->get_post_url(id);

        if (Image::is_valid_extension(image_url))
        {
            m_Images.emplace_back(std::make_shared<Image>(image_path,
                                                          image_url,
                                                          thumb_path,
                                                          thumb_url,
                                                          post_url,
                                                          tags,
                                                          notes_url,
                                                          m_Site,
                                                          *m_ImageFetcher));
        }
        else
        {
            --m_Size;
        }
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
