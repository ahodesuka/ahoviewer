#include "../config.h"

#ifdef HAVE_LIBUNRAR
#include "rar.h"
using namespace AhoViewer;

// Needed for unrar's header with mingw and linux
#ifndef _UNIX
#define _UNIX
#endif // _UNIX

#if defined(HAVE_LIBUNRAR_DLL_HPP)
#include <libunrar/dll.hpp>
#elif defined(HAVE_UNRAR_DLL_HPP)
#include <unrar/dll.hpp>
#endif

std::wstring utf8_to_utf16(const std::string &s)
{
    std::wstring r;
    wchar_t *g = reinterpret_cast<wchar_t*>(g_utf8_to_utf16(s.c_str(), -1, NULL, NULL, NULL));

    if (g)
    {
        r = g;
        g_free(g);
    }

    return r;
}
std::string utf16_to_utf8(const std::wstring &s)
{
    std::string r;
    gchar *g = g_utf16_to_utf8(reinterpret_cast<const gunichar2*>(s.c_str()), -1, NULL, NULL, NULL);

    if (g)
    {
        r = g;
        g_free(g);
    }

    return r;
}

Rar::Rar(const std::string &path, const std::string &exDir)
  : Archive::Archive(path, exDir)
{

}

bool Rar::extract(const std::string &file) const
{
    bool found = false;
    RAROpenArchiveDataEx archive;
    RARHeaderDataEx header;
    memset(&archive, 0, sizeof(archive));

#ifdef _WIN32
    std::wstring wPath  = utf8_to_utf16(m_Path),
                 wEPath = utf8_to_utf16(m_ExtractedPath);
    archive.ArcNameW = const_cast<wchar_t*>(wPath.c_str());
#else
    archive.ArcName  = const_cast<char*>(m_Path.c_str());
#endif // _WIN32
    archive.OpenMode = RAR_OM_EXTRACT;

    HANDLE rar = RAROpenArchiveEx(&archive);

    if (rar)
    {
        while (RARReadHeaderEx(rar, &header) == ERAR_SUCCESS)
        {
#ifdef _WIN32
            std::string filename = utf16_to_utf8(header.FileNameW);
#else
            std::string filename = header.FileName;
#endif // _WIN32
            if (filename == file)
            {
#ifdef _WIN32
                RARProcessFileW(rar, RAR_EXTRACT, const_cast<wchar_t*>(wEPath.c_str()), NULL);
#else
                RARProcessFile(rar, RAR_EXTRACT, const_cast<char*>(m_ExtractedPath.c_str()), NULL);
#endif // _WIN32
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
    RAROpenArchiveDataEx archive;
    RARHeaderDataEx header;
    memset(&archive, 0, sizeof(archive));

#ifdef _WIN32
    std::wstring wPath  = utf8_to_utf16(m_Path);
    archive.ArcNameW = const_cast<wchar_t*>(wPath.c_str());
#else
    archive.ArcName  = const_cast<char*>(m_Path.c_str());
#endif // _WIN32

    archive.OpenMode = RAR_OM_LIST;

    HANDLE rar = RAROpenArchiveEx(&archive);

    if (rar)
    {
        while (RARReadHeaderEx(rar, &header) == ERAR_SUCCESS)
        {
#ifdef _WIN32
            std::string filename = utf16_to_utf8(header.FileNameW);
#else
            std::string filename = header.FileName;
#endif // _WIN32
            if (((t & IMAGES)  && Image::is_valid_extension(filename)) ||
                ((t & ARCHIVES) && Archive::is_valid_extension(filename)))
                entries.push_back(std::move(filename));

            RARProcessFile(rar, RAR_SKIP, NULL, NULL);
        }

        RARCloseArchive(rar);
    }

    return entries;
}
#endif // HAVE_LIBUNRAR
