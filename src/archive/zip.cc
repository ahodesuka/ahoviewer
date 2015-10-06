#include "../config.h"

#ifdef HAVE_LIBZIP
#include "zip.h"
using namespace AhoViewer;

#include <fstream>
#include <iostream>
#include <zip.h>

const char Zip::Magic[Zip::MagicSize] = { 'P', 'K', 0x03, 0x04 };

Zip::Zip(const std::string &path, const std::string &exDir, const std::string &parentDir)
  : Archive::Archive(path, exDir, parentDir)
{

}

void Zip::extract()
{
    if (m_Extracted) return;

    zip *zip = zip_open(m_Path.c_str(), 0, NULL);

    if (zip)
    {
        for (size_t i = 0, n = zip_get_num_entries(zip, 0); i < n; ++i)
        {
            struct zip_stat st;
            zip_stat_init(&st);

            if (zip_stat_index(zip, i, 0, &st) == -1)
            {
                std::cerr << "zip_stat_index: Failed to stat file #" << i
                          << " in '" + m_Path + "'" << std::endl;
                break;
            }

            std::string fPath(Glib::build_filename(m_ExtractedPath, st.name));

            if (fPath.back() == '/')
            {
                g_mkdir_with_parents(fPath.c_str(), 0755);
            }
            else if (Image::is_valid_extension(fPath) || (!m_Child && Archive::is_valid_extension(fPath)))
            {
                zip_file *file = zip_fopen_index(zip, i, 0);
                if (!file)
                {
                    std::cerr << "zip_fopen_index: Failed to open file #" << i
                              << " (" << st.name << ") in '" + m_Path + "'" << std::endl;
                    break;
                }

                char *buf = new char[st.size];
                int bufSize;
                if ((bufSize = zip_fread(file, buf, st.size)) != -1)
                {
                    try
                    {
                        Glib::file_set_contents(fPath, buf, bufSize);
                    }
                    catch(const Glib::FileError &ex)
                    {
                        std::cerr << "Glib::file_set_contents: " << ex.what() << std::endl;
                    }
                }

                delete[] buf;
                zip_fclose(file);
            }

            m_SignalProgress(i + 1, n);
        }

        zip_close(zip);
    }
    else
    {
        std::cerr << "zip_open: Failed to open '" + m_Path + "'" << std::endl;
    }

    m_Extracted = true;
}

bool Zip::has_valid_files(const FileType t) const
{
    return get_n_valid_files(t) > 0;
}

size_t Zip::get_n_valid_files(const FileType t) const
{
    size_t num = 0;
    zip *zip = zip_open(m_Path.c_str(), 0, NULL);

    if (zip)
    {
        for (size_t i = 0, n = zip_get_num_entries(zip, 0); i < n; ++i)
        {
            struct zip_stat st;
            zip_stat_init(&st);

            if (zip_stat_index(zip, i, 0, &st) != -1 &&
                 (((t & IMAGES)   && Image::is_valid_extension(st.name)) ||
                  ((t & ARCHIVES) && Archive::is_valid_extension(st.name))))
                ++num;
        }

        zip_close(zip);
    }

    return num;
}
#endif // HAVE_LIBZIP
