#include "../config.h"

#ifdef HAVE_LIBUNRAR
#include "rar.h"
using namespace AhoViewer;

#ifndef _UNIX
#define _UNIX
#endif // _UNIX

#if defined(HAVE_LIBUNRAR_DLL_HPP)
#include <libunrar/dll.hpp>
#elif defined(HAVE_UNRAR_DLL_HPP)
#include <unrar/dll.hpp>
#endif

const char Rar::Magic[Rar::MagicSize] = { 'R', 'a', 'r', '!', 0x1A, 0x07 };

Rar::Rar(const Glib::ustring &path, const Glib::ustring &exDir)
  : Archive::Archive(path, exDir)
{

}

bool Rar::extract(const Glib::ustring &file) const
{
    bool found = false;
    RAROpenArchiveData archive;
    RARHeaderData header;
    memset(&archive, 0, sizeof(archive));

    archive.ArcName  = const_cast<char*>(m_Path.c_str());
    archive.OpenMode = RAR_OM_EXTRACT;

    HANDLE rar = RAROpenArchive(&archive);

    if (rar)
    {
        while (RARReadHeader(rar, &header) == 0)
        {
            if (header.FileName == file)
            {
                RARProcessFile(rar, RAR_EXTRACT, const_cast<char*>(m_ExtractedPath.c_str()), NULL);
                found = true;
                break;
            }
            else
            {
                RARProcessFile(rar, RAR_SKIP, NULL, NULL);
            }
        }

        RARCloseArchive(rar);
    }

    return found;
}

bool Rar::has_valid_files(const FileType t) const
{
    return !get_entries(t).empty();
}

std::vector<std::string> Rar::get_entries(const FileType t) const
{
    std::vector<std::string> entries;
    RAROpenArchiveData archive;
    RARHeaderData header;
    memset(&archive, 0, sizeof(archive));

    archive.ArcName  = const_cast<char*>(m_Path.c_str());
    archive.OpenMode = RAR_OM_LIST;

    HANDLE rar = RAROpenArchive(&archive);

    if (rar)
    {
        while (RARReadHeader(rar, &header) == 0)
        {
            if (((t & IMAGES)  && Image::is_valid_extension(header.FileName)) ||
                ((t & ARCHIVES) && Archive::is_valid_extension(header.FileName)))
                entries.emplace_back(header.FileName);

            RARProcessFile(rar, RAR_SKIP, NULL, NULL);
        }

        RARCloseArchive(rar);
    }

    return entries;
}
#endif // HAVE_LIBUNRAR
