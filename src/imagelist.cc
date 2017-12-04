#include <numeric>
#include <thread>

#include "imagelist.h"
using namespace AhoViewer;

#include "booru/image.h"
#include "naturalsort.h"
#include "settings.h"

ImageList::ImageList(Widget *const w)
  : m_Widget(w),
    m_Index(0),
    m_ThumbnailCancel(Gio::Cancellable::create()),
    // number of threads (cores) times 2 since most of the work is I/O
    m_ThreadPool(std::thread::hardware_concurrency() * 2),
    m_CacheCancel(Gio::Cancellable::create())
{
    // Sorts indices based on how close they are to m_Index
    m_IndexSort = [=](size_t a, size_t b)
    {
        size_t adiff = std::abs(static_cast<int>(a - m_Index)),
               bdiff = std::abs(static_cast<int>(b - m_Index));
        return adiff == bdiff ? a > b : adiff < bdiff;
    };

    m_Widget->signal_selected_changed().connect(
            sigc::bind(sigc::mem_fun(*this, &ImageList::set_current), true, false));

    m_ThumbnailLoadedConn = m_SignalThumbnailLoaded.connect(sigc::mem_fun(*this, &ImageList::on_thumbnail_loaded));
}

ImageList::~ImageList()
{
    reset();
}

void ImageList::clear()
{
    reset();
    m_SignalCleared();
}

// Creates a local image list from a given file (archive/image) or direcotry.
// The parameter index is used when reopening an archive at a given index.
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
            dirPath = archive->get_extracted_path();
        }
        else
        {
            error = "'" + Glib::path_get_basename(path) + "' is invalid or not supported.";
            return false;
        }
    }
    else
    {
        error = "File or directory '" + path + "' could not be opened.";
        return false;
    }

    std::vector<std::string> entries =
        archive ? archive->get_entries(Archive::IMAGES) : get_entries<Image>(dirPath);

    // No valid images in this directory
    if (entries.empty())
    {
        error = "No valid image files found in '" +
            Glib::path_get_basename(archive ? archive->get_path() : dirPath.c_str()) + "'.";
        return false;
    }

    m_SignalLoadSuccess();
    reset();

    // Create the actual vector of images
    m_Images.reserve(entries.size());
    m_Widget->reserve(entries.size());

    if (archive)
    {
        m_Archive = std::move(archive);
        m_ArchiveEntries = get_entries<Archive>(Glib::path_get_dirname(m_Archive->get_path()));
        std::sort(m_ArchiveEntries.begin(), m_ArchiveEntries.end(), NaturalSort());
    }
    else
    {
        Glib::RefPtr<Gio::File> dir = Gio::File::create_for_path(dirPath);
        m_FileMonitor = dir->monitor_directory();
        m_FileMonitor->signal_changed().connect(sigc::mem_fun(*this, &ImageList::on_directory_changed));
    }

    std::sort(entries.begin(), entries.end(), NaturalSort());

    if (path != dirPath && !m_Archive)
    {
        std::vector<std::string>::iterator iter = std::find(entries.begin(), entries.end(), path);
        index = iter == entries.end() ? 0 : iter - entries.begin();
    }
    else if (index == -1)
    {
        index = entries.size() - 1;
    }

    for (const std::string &e : entries)
    {
        std::shared_ptr<Image> img;
        if (m_Archive)
            img = std::make_shared<Archive::Image>(e, *m_Archive);
        else
            img = std::make_shared<Image>(e);
        m_Images.push_back(std::move(img));
    }

    set_current(index, false, true);
    m_ThumbnailThread = std::thread(sigc::mem_fun(*this, &ImageList::load_thumbnails));

    return true;
}

void ImageList::go_next()
{
    set_current_relative(1);
}

void ImageList::go_previous()
{
    set_current_relative(-1);
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
    if (m_Index > 0)
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

void ImageList::set_current(const size_t index, const bool fromWidget, const bool force)
{
    if (index == m_Index && !force)
        return;

    m_Index = index;
    m_SignalChanged(m_Images[m_Index]);
    update_cache();

    if (m_ThreadPool.active())
    {
        cancel_thumbnail_thread();
        m_ThumbnailThread = std::thread(sigc::mem_fun(*this, &ImageList::load_thumbnails));
    }

    if (!fromWidget)
        m_Widget->set_selected(m_Index);
}

void ImageList::load_thumbnails()
{
    m_ThumbnailCancel->reset();

    std::vector<size_t> indices(m_Images.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), m_IndexSort);

    for (const size_t i : indices)
    {
        m_ThreadPool.push([ &, i ]()
        {
            if (m_ThumbnailCancel->is_cancelled())
                return;

            const Glib::RefPtr<Gdk::Pixbuf> thumb = m_Images[i]->get_thumbnail();
            m_ThumbnailQueue.push(PixbufPair(i, std::move(thumb)));

            if (!m_ThumbnailCancel->is_cancelled())
                m_SignalThumbnailLoaded();
        });
    }
}

// Resets the image list to it's initial state
void ImageList::reset()
{
    cancel_cache();

    if (m_FileMonitor)
    {
        m_FileMonitor->cancel();
        m_FileMonitor.reset();
    }

    cancel_thumbnail_thread();

    m_Images.clear();
    m_Widget->clear();

    m_Archive = nullptr;
    m_ArchiveEntries.clear();
    m_Index = 0;
}

void ImageList::cancel_thumbnail_thread()
{
    m_ThumbnailCancel->cancel();

    m_ThreadPool.kill();
    if (m_ThumbnailThread.joinable())
        m_ThumbnailThread.join();

    m_ThumbnailQueue.clear();
}

// Returns an unsorted vector of the paths to valid T's.
// T must have a static method ::is_valid_extension, ie Image and Archive
template <typename T>
std::vector<std::string> ImageList::get_entries(const std::string &path)
{
    Glib::Dir dir(path);
    std::vector<std::string> entries(dir.begin(), dir.end());
    std::vector<std::string>::iterator i = entries.begin();

    while (i != entries.end())
    {
        // Make path absolute
        *i = Glib::build_filename(path, *i);

        // Make sure it is a loadable image/archive
        if (T::is_valid_extension(*i))
            ++i;
        else
            i = entries.erase(i);
    }

    return entries;
}

void ImageList::on_thumbnail_loaded()
{
    m_ThumbnailLoadedConn.block();
    PixbufPair p;

    while (!m_ThumbnailCancel->is_cancelled() && m_ThumbnailQueue.pop(p))
        m_Widget->set_pixbuf(p.first, p.second);

    m_ThumbnailLoadedConn.unblock();
}

void ImageList::on_directory_changed(const Glib::RefPtr<Gio::File> &file,
                                     const Glib::RefPtr<Gio::File>&,
                                     Gio::FileMonitorEvent event)
{
    if (!file) return;

    ImageVector::iterator it;
    auto comp = [ file ](const std::shared_ptr<Image> &i) { return i->get_path() == file->get_path(); };

    if (event == Gio::FILE_MONITOR_EVENT_DELETED)
    {
        if ((it = std::find_if(m_Images.begin(), m_Images.end(), comp)) != m_Images.end())
        {
            size_t index = it - m_Images.begin();
            bool current = index == m_Index;

            // Adjust m_Index if the deleted file was before it
            if (index < m_Index)
                --m_Index;

            m_Widget->erase(index);
            m_Images.erase(it);

            if (current)
            {
                if (m_Images.empty())
                {
                    clear();
                    return;
                }
                else
                {
                    set_current(index == 0 ? 0 : index - 1, false, true);
                }
            }
            else
            {
                update_cache();
            }

            m_SignalSizeChanged();
        }
        else if (file->get_path() == Glib::path_get_dirname(get_current()->get_path()))
        {
            clear();
        }
    }
    // The changed event is used in case the created event was too quick,
    // and the file was invalid while still being written
    else if ((event == Gio::FILE_MONITOR_EVENT_CREATED ||
              event == Gio::FILE_MONITOR_EVENT_CHANGES_DONE_HINT) &&
            Image::is_valid(file->get_path()))
    {
        // Make sure the image wasn't already added
        if (event == Gio::FILE_MONITOR_EVENT_CHANGES_DONE_HINT &&
            std::find_if(m_Images.begin(), m_Images.end(), comp) != m_Images.end())
            return;

        std::shared_ptr<Image> img = std::make_shared<Image>(file->get_path());
        it = std::lower_bound(m_Images.begin(), m_Images.end(), img, NaturalSort());
        size_t index = it - m_Images.begin();

        if (index <= m_Index)
            ++m_Index;

        m_Widget->insert(index, img->get_thumbnail());
        m_Images.insert(it, img);

        update_cache();
        m_SignalSizeChanged();
    }
}

void ImageList::set_current_relative(const int d)
{
    if ((d > 0 && m_Index + 1 < m_Images.size()) || (d < 0 && m_Index > 0))
    {
        set_current(m_Index + d);
    }
    else if (m_Archive && Settings.get_bool("AutoOpenArchive"))
    {
        size_t i = std::find(m_ArchiveEntries.begin(), m_ArchiveEntries.end(),
                        m_Archive->get_path()) - m_ArchiveEntries.begin();

        if ((d > 0 && i < m_ArchiveEntries.size() - 1) || (d < 0 && i > 0))
        {
            std::string e;
            if (!load(m_ArchiveEntries[i + d], e, d < 0 ? -1 : 0))
                m_SignalArchiveError(e);
        }
    }
}

void ImageList::update_cache()
{
    std::vector<size_t> cache(m_Images.size()), diff;
    std::iota(cache.begin(), cache.end(), 0);
    std::sort(cache.begin(), cache.end(), m_IndexSort);

    cache.resize(Settings.get_int("CacheSize") * 2 + 1);

    // Get the indices of the images no longer in the cache
    if (!m_Cache.empty())
    {
        // Copy the cache to preserve the desired sort order
        std::vector<size_t> tmp = cache;
        std::sort(m_Cache.begin(), m_Cache.end());
        std::sort(tmp.begin(), tmp.end());
        std::set_difference(m_Cache.begin(), m_Cache.end(),
                            tmp.begin(), tmp.end(), std::back_inserter(diff));
    }

    cancel_cache();

    for (const size_t i : diff)
        if (i < m_Images.size() - 1)
            m_Images[i]->reset_pixbuf();

    // Start the cache loading thread
    // TODO: Make a persistent thread class that does work when needed and
    // waits on a coditional variable (returns it) can be used here and for
    // other places where a single thread is used frequently
    m_Cache = cache;
    m_CacheThread = std::thread([&]()
    {
        for (const size_t i : m_Cache)
        {
            if (m_CacheCancel->is_cancelled())
                break;

            m_Images[i]->load_pixbuf();
        }
    });
}

void ImageList::cancel_cache()
{
    if (m_CacheThread.joinable())
    {
        m_CacheCancel->cancel();

        m_CacheThread.join();
        m_CacheCancel->reset();

        m_Cache.clear();
    }
}
