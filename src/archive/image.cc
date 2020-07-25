#include <giomm.h>

#include "archive.h"
using namespace AhoViewer;

Archive::Image::Image(const std::string &path, const Archive &archive)
  : AhoViewer::Image(Glib::build_filename(archive.get_extracted_path(), path)),
    m_ArchiveFilePath(path),
    m_Archive(archive)
{

}

std::string Archive::Image::get_filename() const
{
    return Glib::build_filename(Glib::path_get_basename(m_Archive.get_path()),
                                Glib::path_get_basename(m_Path));
}

const Glib::RefPtr<Gdk::Pixbuf>& Archive::Image::get_thumbnail(Glib::RefPtr<Gio::Cancellable> c)
{
    if (!m_ThumbnailPixbuf)
    {
        extract_file();
        create_thumbnail(c, false);
    }

    return m_ThumbnailPixbuf;
}

void Archive::Image::load_pixbuf(Glib::RefPtr<Gio::Cancellable> c)
{
    if (!m_Pixbuf)
    {
        extract_file();
        AhoViewer::Image::load_pixbuf(c);
    }
}

void Archive::Image::save(const std::string &path)
{
    Glib::RefPtr<Gio::File> src{ Gio::File::create_for_path(m_Path) },
                            dst{ Gio::File::create_for_path(path) };
    src->copy(dst, Gio::FILE_COPY_OVERWRITE);
}

void Archive::Image::extract_file()
{
    // This lock is needed to prevent the cache thread and
    // the thumbnail thread from stepping on each other's toes
    std::scoped_lock lock{ m_Mutex };
    if (!Glib::file_test(m_Path, Glib::FILE_TEST_EXISTS))
        m_Archive.extract(m_ArchiveFilePath);
}
