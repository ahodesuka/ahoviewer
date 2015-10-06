#include <cctype>
#include <cstring>
#include <fstream>

#include "archive.h"
using namespace AhoViewer;

#include "config.h"
#include "tempdir.h"
#include "rar.h"
#include "zip.h"

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

bool Archive::is_valid(const std::string &path)
{
    return get_type(path) != Type::UNKNOWN;
}

bool Archive::is_valid_extension(const std::string &path)
{
    std::string ext = path.substr(path.find_last_of('.') + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    for (const std::string &e : FileExtensions)
        if (ext == e)
            return true;

    return false;
}

std::unique_ptr<Archive> Archive::create(const std::string &path, const std::string &parentDir)
{
    std::string dir;
    Type type = get_type(path);

    if (type != Type::UNKNOWN)
    {
        dir = TempDir::get_instance().make_dir(
            !parentDir.empty() ? Glib::build_filename(Glib::path_get_basename(parentDir),
                                                      Glib::path_get_basename(path)) :
                                 Glib::path_get_basename(path));
    }

    if (!dir.empty())
    {
        if (type == Type::ZIP)
        {
            return std::unique_ptr<Archive>(new Zip(path, dir, parentDir));
        }
        else if (type == Type::RAR)
        {
            return std::unique_ptr<Archive>(new Rar(path, dir, parentDir));
        }
    }

    return nullptr;
}

Archive::Type Archive::get_type(const std::string &path)
{
    std::ifstream ifs(path, std::ios::binary);

    char magic[MagicSize];
    ifs.read(magic, sizeof(magic) / sizeof(*magic));

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

Archive::Archive(const std::string &path, const std::string &exDir, const std::string &parentDir)
  : m_Path(path),
    m_ExtractedPath(exDir),
    m_Child(!parentDir.empty()),
    m_Extracted(false)
{

}

Archive::~Archive()
{
    m_Children.clear();
    TempDir::get_instance().remove_dir(m_ExtractedPath);
}
