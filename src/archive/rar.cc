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

Rar::Rar(const std::string &path, const std::string &exDir, const std::string &parentDir)
  : Archive::Archive(path, exDir, parentDir)
{

}

void Rar::extract()
{
    if (m_Extracted) return;

    RAROpenArchiveData archive;
    RARHeaderData header;
    memset(&archive, 0, sizeof(archive));

    archive.ArcName  = const_cast<char*>(m_Path.c_str());
    archive.OpenMode = RAR_OM_EXTRACT;

    HANDLE rar = RAROpenArchive(&archive);

    if (rar)
    {
        size_t nFiles = get_n_valid_files(),
               nExtracted = 0;

        while (RARReadHeader(rar, &header) == 0)
        {
            if (Image::is_valid_extension(header.FileName) || (!m_Child && Archive::is_valid_extension(header.FileName)))
            {
                RARProcessFile(rar, RAR_EXTRACT, const_cast<char*>(m_ExtractedPath.c_str()), NULL);
                m_SignalProgress(++nExtracted, nFiles);
            }
            else
            {
                RARProcessFile(rar, RAR_SKIP, NULL, NULL);
            }
        }

        RARCloseArchive(rar);
    }

    m_Extracted = true;
}

bool Rar::has_valid_files(const FileType t) const
{
    return get_n_valid_files(t) > 0;
}

size_t Rar::get_n_valid_files(const FileType t) const
{
    size_t num = 0;
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
                ++num;

            RARProcessFile(rar, RAR_SKIP, NULL, NULL);
        }

        RARCloseArchive(rar);
    }

    return num;
}
#endif // HAVE_LIBUNRAR
