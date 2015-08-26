#include <cstring>
#include <fstream>

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

const std::map<Archive::Type, const Archive::Extractor *const> Archive::Extractors =
{
#ifdef HAVE_LIBZIP
    { Type::ZIP, new Zip },
#endif // HAVE_LIBZIP

#ifdef HAVE_LIBUNRAR
    { Type::RAR, new Rar },
#endif // HAVE_LIBUNRAR
};

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

std::shared_ptr<Archive> Archive::create(const std::string &path, const std::shared_ptr<Archive> &parent)
{
    std::shared_ptr<Archive> archive = nullptr;
    Type type = get_type(path);

    if (Extractors.find(type) != Extractors.end())
    {
        std::string extractedPath(Extractors.at(type)->extract(path, parent));

        if (!extractedPath.empty())
        {
            archive = std::make_shared<Archive>(path, extractedPath);

            if (parent)
                parent->m_Children.push_back(archive);
        }
    }

    return archive;
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

Archive::Archive(const std::string &path, const std::string &extractedPath)
  : m_Path(path),
    m_ExtractedPath(extractedPath)
{

}

Archive::~Archive()
{
    m_Children.clear();
    TempDir::get_instance().remove_dir(m_ExtractedPath);
}
