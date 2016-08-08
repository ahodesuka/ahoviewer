#include <cctype>
#include <cstring>
#include <fstream>
#include <giomm.h>
#include <glib.h>

#include "archive.h"
using namespace AhoViewer;

#include "config.h"
#include "tempdir.h"
#ifdef HAVE_LIBUNRAR
#include "rar.h"
#endif // HAVE_LIBUNRAR
#ifdef HAVE_LIBZIP
#include "zip.h"
#endif // HAVE_LIBZIP

const std::vector<std::string> Archive::MimeTypes =
{
#ifdef HAVE_LIBZIP
    "application/x-zip",
    "application/x-zip-compressed",
    "application/zip",
    "application/cbz",
#endif // HAVE_LIBZIP

#ifdef HAVE_LIBUNRAR
    "application/x-rar",
    "application/x-rar-compressed",
    "application/x-cbr",
#endif // HAVE_LIBUNRAR
};

const std::vector<std::string> Archive::FileExtensions =
{
#ifdef HAVE_LIBZIP
    "zip",
    "cbz",
#endif // HAVE_LIBZIP

#ifdef HAVE_LIBUNRAR
    "rar",
    "cbr",
#endif // HAVE_LIBUNRAR
};

bool Archive::is_valid(const Glib::ustring &path)
{
    return get_type(path) != Type::UNKNOWN;
}

bool Archive::is_valid_extension(const Glib::ustring &path)
{
    std::string ext = path.substr(path.find_last_of('.') + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    for (const std::string &e : FileExtensions)
        if (ext == e)
            return true;

    return false;
}

std::unique_ptr<Archive> Archive::create(const Glib::ustring &path, const Glib::ustring &parentDir)
{
    Glib::ustring dir;
    Type type = get_type(path);

    if (type != Type::UNKNOWN)
    {
        dir = TempDir::get_instance().make_dir(
            !parentDir.empty() ? Glib::build_filename(Glib::path_get_basename(parentDir),
                                                      Glib::path_get_basename(path)) :
                                 Glib::path_get_basename(path));

        // Make sure temp directory was created
        if (!dir.empty())
        {
#ifdef HAVE_LIBZIP
            if (type == Type::ZIP)
                return std::unique_ptr<Archive>(new Zip(path, dir));
#endif // HAVE_LIBZIP

#ifdef HAVE_LIBUNRAR
            if (type == Type::RAR)
                return std::unique_ptr<Archive>(new Rar(path, dir));
#endif // HAVE_LIBUNRAR
        }
    }

    return nullptr;
}

Archive::Type Archive::get_type(const Glib::ustring &path)
{
    if (Glib::file_test(path, Glib::FILE_TEST_IS_DIR))
        return Type::UNKNOWN;

    Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(path);
    Glib::RefPtr<Gio::FileInputStream> ifs = file->read();

    char magic[MagicSize] = { };
    ifs->read(magic, MagicSize);

#ifdef HAVE_LIBZIP
    if (std::memcmp(magic, Zip::Magic, Zip::MagicSize) == 0)
        return Type::ZIP;
#endif // HAVE_LIBZIP

#ifdef HAVE_LIBUNRAR
    if (std::memcmp(magic, Rar::Magic, Rar::MagicSize) == 0)
        return Type::RAR;
#endif // HAVE_LIBUNRAR

    return Type::UNKNOWN;
}

Archive::Archive(const Glib::ustring &path, const Glib::ustring &exDir)
  : m_Path(path),
    m_ExtractedPath(exDir)
{
#ifdef _WIN32
    gchar *tmp = g_win32_locale_filename_from_utf8(path.data());
    m_Path = tmp;

    g_free(tmp);
#endif // _WIN32
}

Archive::~Archive()
{
    TempDir::get_instance().remove_dir(m_ExtractedPath);
}
