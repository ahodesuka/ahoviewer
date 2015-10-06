#include "archive.h"
using namespace AhoViewer;

Archive::Image::Image(const std::string &path, const Archive &archive)
  : AhoViewer::Image(path),
    m_Archive(archive)
{

}

#include <iostream>
std::string Archive::Image::get_filename() const
{
    return Glib::build_filename(Glib::path_get_basename(m_Archive.get_path()),
                                Glib::path_get_basename(m_Path));
}

const Glib::RefPtr<Gdk::Pixbuf>& Archive::Image::get_thumbnail()
{
    if (!m_ThumbnailPixbuf)
        create_thumbnail();

    return m_ThumbnailPixbuf;
}
