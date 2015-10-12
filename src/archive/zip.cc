#include "../config.h"

#ifdef HAVE_LIBZIP
#include "zip.h"
using namespace AhoViewer;

#include <fstream>
#include <iostream>
#include <zip.h>

const char Zip::Magic[Zip::MagicSize] = { 'P', 'K', 0x03, 0x04 };

Zip::Zip(const std::string &path, const std::string &exDir)
  : Archive::Archive(path, exDir)
{

}

bool Zip::extract(const std::string &file) const
{
    bool found = false;
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

            if (st.name == file)
            {
                std::string fPath(Glib::build_filename(m_ExtractedPath, st.name));

                if (!Glib::file_test(Glib::path_get_dirname(fPath), Glib::FILE_TEST_EXISTS))
                    g_mkdir_with_parents(Glib::path_get_dirname(fPath).c_str(), 0755);


                zip_file *zfile = zip_fopen_index(zip, i, 0);
                if (!zfile)
                {
                    std::cerr << "zip_fopen_index: Failed to open file #" << i
                              << " (" << st.name << ") in '" + m_Path + "'" << std::endl;
                    break;
                }

                char *buf = new char[st.size];
                int bufSize;
                if ((bufSize = zip_fread(zfile, buf, st.size)) != -1)
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
                zip_fclose(zfile);
                found = true;
                break;
            }
        }

        zip_close(zip);
    }
    else
    {
        std::cerr << "zip_open: Failed to open '" + m_Path + "'" << std::endl;
    }

    return found;
}

bool Zip::has_valid_files(const FileType t) const
{
    return !get_entries(t).empty();
}

std::vector<std::string> Zip::get_entries(const FileType t) const
{
    std::vector<std::string> entries;
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
                entries.push_back(st.name);
        }

        zip_close(zip);
    }

    return entries;
}
#endif // HAVE_LIBZIP
