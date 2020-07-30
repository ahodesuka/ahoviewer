#include "../config.h"

#ifdef HAVE_LIBZIP
#include "zip.h"
using namespace AhoViewer;

#include <iostream>
#include <giomm.h>
#include <zip.h>

Zip::Zip(const std::string &path, const std::string &exDir)
  : Archive::Archive(path, exDir)
{

}

bool Zip::extract(const std::string &file) const
{
    bool found{ false };
    zip *zip{ zip_open(m_Path.c_str(), ZIP_RDONLY | ZIP_CHECKCONS, NULL) };

    if (zip)
    {
        struct zip_stat st;
        zip_stat_init(&st);

        if (zip_stat(zip, file.c_str(), 0, &st) == 0)
        {
            std::string fPath{ Glib::build_filename(m_ExtractedPath, st.name) };

            if (!Glib::file_test(Glib::path_get_dirname(fPath), Glib::FILE_TEST_EXISTS))
                g_mkdir_with_parents(Glib::path_get_dirname(fPath).c_str(), 0755);


            zip_file *zfile{ zip_fopen(zip, file.c_str(), 0) };
            if (zfile)
            {
                std::vector<char> buf(st.size);
                zip_int64_t bufSize{ zip_fread(zfile, buf.data(), st.size) };
                if (bufSize != -1)
                {
                    auto f{ Gio::File::create_for_path(fPath) };
                    std::string etag;
                    f->replace_contents(buf.data(), bufSize, "", etag);
                }

                zip_fclose(zfile);
                found = true;
            }
            else
            {
                std::cerr << "zip_fopen_index: Failed to open file "
                          << st.name << " in '" + m_Path + "'" << std::endl;
            }
        }
        else
        {
            std::cerr << "zip_stat_index: Failed to stat file " << file
                      << " in '" + m_Path + "'" << std::endl;
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
    zip *zip{ zip_open(m_Path.c_str(), ZIP_RDONLY | ZIP_CHECKCONS, NULL) };

    if (zip)
    {
        for (zip_int64_t i{ 0 }, n{ zip_get_num_entries(zip, 0) }; i < n; ++i)
        {
            struct zip_stat st;
            zip_stat_init(&st);

            if (zip_stat_index(zip, i, 0, &st) != -1 &&
                 (((t & IMAGES)   && Image::is_valid_extension(st.name)) ||
                  ((t & ARCHIVES) && Archive::is_valid_extension(st.name))))
                entries.emplace_back(st.name);
        }

        zip_close(zip);
    }

    return entries;
}
#endif // HAVE_LIBZIP
