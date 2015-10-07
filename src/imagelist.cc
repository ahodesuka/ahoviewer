#include "imagelist.h"
using namespace AhoViewer;

#include <glib/gstdio.h>

#include "booru/image.h"
#include "naturalsort.h"
#include "settings.h"

ImageList::ImageList(Widget *w)
  : m_Widget(w),
    m_Index(0),
    m_ThumbnailCancel(Gio::Cancellable::create()),
    m_ThumbnailThread(nullptr),
    m_CacheCancel(Gio::Cancellable::create()),
    m_CacheThread(nullptr)
{
    m_Widget->signal_selected_changed().connect(
            sigc::bind<1>(sigc::mem_fun(*this, &ImageList::set_current), true));

    m_SignalThumbnailLoaded.connect(sigc::mem_fun(*this, &ImageList::on_thumbnail_loaded));
    m_SignalThumbnailsLoaded.connect(sigc::mem_fun(*this, &ImageList::on_thumbnails_loaded));
}

ImageList::~ImageList()
{
    clear();
}

void ImageList::clear()
{
    cancel_cache();
    m_ThumbnailCancel->cancel();

    {
        Glib::Threads::Mutex::Lock lock(m_ThumbnailMutex);
        m_ThumbnailQueue = std::queue<PixbufPair>();
    }

    if (m_ThumbnailThread)
    {
        m_ThumbnailThread->join();
        m_ThumbnailThread = nullptr;
    }

    m_Images.clear();

    {
        Glib::Threads::Mutex::Lock lock(m_ThumbnailMutex);
        m_Widget->clear();
    }

    m_Archive = nullptr;
    m_Index = 0;
    m_SignalCleared();
}

/**
 * Creates a local image list from a given file (archive/image) or direcotry.
 * The parameter index is used when reopening an archive at a given index.
 **/
bool ImageList::load(const std::string path, std::string &error, int index)
{
    std::unique_ptr<Archive> archive = nullptr;
    std::string dirPath;

    if (Glib::file_test(path, Glib::FILE_TEST_EXISTS))
    {
        if (Glib::file_test(path, Glib::FILE_TEST_IS_DIR))
        {
            dirPath = path;
        }
        else if (Image::is_valid(path))
        {
            dirPath = Glib::path_get_dirname(path);
        }
        else if ((archive = Archive::create(path)))
        {
            archive->signal_progress().connect(
                sigc::mem_fun(m_SignalExtractorProgress, &SignalExtractorProgressType::emit));
            archive->extract();
            dirPath = archive->get_extracted_path();
        }
        else
        {
            error = "'" + path + "' is invalid or not supported.";
            return false;
        }
    }
    else
    {
        error = "File or directory '" + path + "' could not be opened.";
        return false;
    }

    std::vector<std::string> entries = get_image_entries(dirPath, !!archive), subEntries;

    if (entries.empty() && archive && !(subEntries = get_archive_entries(dirPath, true)).empty())
    {
        for (const std::string &s : subEntries)
        {
            std::unique_ptr<Archive> arc = Archive::create(s, archive->get_extracted_path());

            if (arc && arc->has_valid_files(Archive::IMAGES))
            {
                arc->signal_progress().connect(
                    sigc::mem_fun(m_SignalExtractorProgress, &SignalExtractorProgressType::emit));
                arc->extract();
                archive->add_child(arc);
            }

            g_unlink(s.c_str());
        }

        entries = get_image_entries(dirPath, true);
    }

    // No valid images in this directory
    if (entries.empty())
    {
        error = "No valid image files found in '" + dirPath + "'.";
        return false;
    }

    m_SignalLoadSuccess();

    if (!archive)
    {
        m_Archive = nullptr;
    }
    else
    {
        m_Archive = std::move(archive);
        m_ArchiveEntries = get_archive_entries(Glib::path_get_dirname(m_Archive->get_path()));
    }

    // Sort entries alphanumerically
    std::sort(entries.begin(), entries.end(), NaturalSort());

    if (path != dirPath && !m_Archive)
    {
        std::vector<std::string>::iterator iter = std::find(entries.begin(), entries.end(), path);
        m_Index = iter == entries.end() ? 0 : iter - entries.begin();
    }
    else if (index == -1)
    {
        m_Index = entries.size() - 1;
    }
    else
    {
        m_Index = index;
    }

    cancel_cache();

    // Create the actual vector of images
    m_Images.clear();
    m_Images.reserve(entries.size());

    m_Widget->clear();
    m_Widget->reserve(entries.size());

    for (const std::string &e : entries)
    {
        std::shared_ptr<Image> img;
        if (m_Archive)
            img = std::make_shared<Archive::Image>(e, *m_Archive);
        else
            img = std::make_shared<Image>(e);
        m_Images.push_back(img);
    }

    m_ThumbnailThread = Glib::Threads::Thread::create(sigc::mem_fun(*this, &ImageList::load_thumbnails));
    set_current(m_Index);

    return true;
}

void ImageList::go_next()
{
    if (m_Index + 1 < m_Images.size())
    {
        set_current(m_Index + 1);
    }
    else if (m_Archive && Settings.get_bool("AutoOpenArchive"))
    {
        size_t i = std::find(m_ArchiveEntries.begin(), m_ArchiveEntries.end(),
                        m_Archive->get_path()) - m_ArchiveEntries.begin();

        if (i < m_ArchiveEntries.size() - 1)
        {
            std::string e;
            if (!load(m_ArchiveEntries[i + 1], e))
                m_SignalArchiveError(e);
        }
    }
}

void ImageList::go_previous()
{
    if (m_Index != 0)
    {
        set_current(m_Index - 1);
    }
    else if (m_Archive && Settings.get_bool("AutoOpenArchive"))
    {
        size_t i = std::find(m_ArchiveEntries.begin(), m_ArchiveEntries.end(),
                        m_Archive->get_path()) - m_ArchiveEntries.begin();

        if (i > 0)
        {
            std::string e;
            if (!load(m_ArchiveEntries[i - 1], e, -1))
                m_SignalArchiveError(e);
        }
    }
}

void ImageList::go_first()
{
    set_current(0);
}

void ImageList::go_last()
{
    set_current(m_Images.size() - 1);
}

bool ImageList::can_go_next() const
{
    if (m_Index + 1 < m_Images.size())
        return true;
    else if (m_Archive && Settings.get_bool("AutoOpenArchive"))
        return std::find(m_ArchiveEntries.begin(), m_ArchiveEntries.end(),
                m_Archive->get_path()) - m_ArchiveEntries.begin() < static_cast<long>(m_ArchiveEntries.size() - 1);

    return false;
}

bool ImageList::can_go_previous() const
{
    if (m_Index != 0)
        return true;
    else if (m_Archive && Settings.get_bool("AutoOpenArchive"))
        return std::find(m_ArchiveEntries.begin(), m_ArchiveEntries.end(),
                m_Archive->get_path()) - m_ArchiveEntries.begin() > 0;

    return false;
}

void ImageList::on_cache_size_changed()
{
    if (!empty())
        update_cache();
}

void ImageList::set_current(const size_t index, const bool fromWidget)
{
    // Ignore clicking
    if (index == m_Index && fromWidget)
        return;

    m_Index = index;
    m_SignalChanged(m_Images[m_Index]);
    update_cache();

    // Do not call this if this was called from a selected_changed signal.
    if (!fromWidget)
        m_Widget->set_selected(m_Index);
}

void ImageList::load_thumbnails()
{
    Glib::ThreadPool pool(4);
    m_ThumbnailCancel->reset();

    for (size_t i = 0; i < m_Images.size(); ++i)
    {
        pool.push([ this, i ]()
        {
            if (m_ThumbnailCancel->is_cancelled())
                return;

            const Glib::RefPtr<Gdk::Pixbuf> &thumb = m_Images[i]->get_thumbnail();
            {
                Glib::Threads::Mutex::Lock lock(m_ThumbnailMutex);
                m_ThumbnailQueue.push(PixbufPair(i, thumb));
            }

            if (!m_ThumbnailCancel->is_cancelled())
                m_SignalThumbnailLoaded();
        });
    }

    pool.shutdown(m_ThumbnailCancel->is_cancelled());

    if (!m_ThumbnailCancel->is_cancelled())
        m_SignalThumbnailsLoaded();
}

std::vector<std::string> ImageList::get_image_entries(const std::string &path, const bool recursive)
{
    Glib::Dir dir(path);
    std::vector<std::string> entries(dir.begin(), dir.end()), rEntries;
    std::vector<std::string>::iterator i = entries.begin();

    while (i != entries.end())
    {
        // Make path absolute
        *i = Glib::build_filename(path, *i);

        // Recurse if this is from an archive
        if (recursive && Glib::file_test(*i, Glib::FILE_TEST_IS_DIR))
        {
            std::vector<std::string> r = get_image_entries(*i, true);
            if (r.size())
                rEntries.insert(rEntries.end(), r.begin(), r.end());
        }

        // Make sure it is a loadable image
        if (Image::is_valid(*i))
            ++i;
        else
            i = entries.erase(i);
    }

    // combine the recured entries
    if (rEntries.size())
        entries.insert(entries.end(), rEntries.begin(), rEntries.end());

    return entries;
}

std::vector<std::string> ImageList::get_archive_entries(const std::string &path, const bool recursive)
{
    Glib::Dir dir(path);
    std::vector<std::string> entries(dir.begin(), dir.end()), rEntries;
    std::vector<std::string>::iterator i = entries.begin();

    while (i != entries.end())
    {
        *i = Glib::build_filename(path, *i);

        if (recursive && Glib::file_test(*i, Glib::FILE_TEST_IS_DIR))
        {
            std::vector<std::string> r = get_archive_entries(*i, true);
            if (r.size())
                rEntries.insert(rEntries.end(), r.begin(), r.end());
        }

        if (Archive::is_valid(*i))
            ++i;
        else
            i = entries.erase(i);
    }

    if (rEntries.size())
        entries.insert(entries.end(), rEntries.begin(), rEntries.end());

    std::sort(entries.begin(), entries.end(), NaturalSort());

    return entries;
}

void ImageList::on_thumbnail_loaded()
{
    Glib::Threads::Mutex::Lock lock(m_ThumbnailMutex);
    while (!m_ThumbnailQueue.empty())
    {
        PixbufPair p = m_ThumbnailQueue.front();
        m_Widget->set_pixbuf(p.first, p.second);
        m_ThumbnailQueue.pop();
    }
}

void ImageList::on_thumbnails_loaded()
{
    m_ThumbnailThread->join();
    m_ThumbnailThread = nullptr;

    m_Widget->on_thumbnails_loaded(m_Index);
}

void ImageList::update_cache()
{
    std::vector<size_t> cache = { m_Index }, diff;
    size_t ncount = 0, pcount = 0;

    // example: cacheSize = 2, index = 0
    // cache = { 0, 1, 2, 3, 4 }
    // example: cacheSize = 3, index = 4
    // cache = { 4, 5, 6, 7, 3, 2, 1 }
    // example: cacheSize = 2, index = 9, size = 11
    // cache = { 9, 10, 8, 7, 6 }
    for (size_t i = 1, cacheSize = Settings.get_int("CacheSize"); i < cacheSize + 1; ++i)
    {
        if (m_Index + i < m_Images.size())
        {
            cache.push_back(m_Index + i);
            ++ncount;
        }
        else if (static_cast<int>(m_Index - i - cacheSize) >= 0)
        {
            cache.push_back(m_Index - i - cacheSize + ncount);
        }

        if (static_cast<int>(m_Index - i) >= 0)
        {
            cache.push_back(m_Index - i);
            ++pcount;
        }
        else if (m_Index + i + cacheSize < m_Images.size())
        {
            cache.push_back(m_Index + i + cacheSize - pcount);
        }
    }

    // Get the indices of the images no longer in the cache
    if (!m_Cache.empty())
    {
        std::sort(m_Cache.begin(), m_Cache.end());
        std::sort(cache.begin(), cache.end());
        std::set_difference(m_Cache.begin(), m_Cache.end(),
                cache.begin(), cache.end(), std::back_inserter(diff));
    }

    // Sort by how close a/b is to the current index
    std::sort(cache.begin(), cache.end(), [ this ](size_t a, size_t b)
    {
        return (a < m_Index ? std::abs(static_cast<int>(a - m_Index)) + 1e18 : a) <
               (b < m_Index ? std::abs(static_cast<int>(b - m_Index)) + 1e18 : b);
    });

    cancel_cache();

    for (const size_t i : diff)
        m_Images[i]->reset_pixbuf();

    // Start the cache loading thread
    m_CacheThread = Glib::Threads::Thread::create([ this, cache ]()
    {
        for (const size_t i : cache)
        {
            if (m_CacheCancel->is_cancelled())
                break;

            m_Images[i]->load_pixbuf();
        }
    });

    m_Cache = cache;
}

void ImageList::cancel_cache()
{
    if (m_CacheThread)
    {
        m_CacheCancel->cancel();

        m_CacheThread->join();
        m_CacheThread = nullptr;
        m_CacheCancel->reset();

        m_Cache.clear();
    }
}
