#include <fstream>
#include <iostream>

#include "../config.h"
#include "../tempdir.h"

#ifdef HAVE_LIBZIP
#include "zip.h"
using namespace AhoViewer;

const char Zip::Magic[Zip::MagicSize] = { 'P', 'K', 0x03, 0x04 };

std::string Zip::extract(const std::string &path) const
{
    std::string extractedPath;
    zip_archive *zip = zip_open(path.c_str(), 0, NULL);

    if (zip)
    {
        const zip_int64_t nEntries = zip_get_num_entries(zip, 0);
        extractedPath = TempDir::get_instance().make_dir(Glib::path_get_basename(path));

        // Failed to create temp dir
        if (!extractedPath.empty())
        {
            for (zip_int64_t i = 0; i < nEntries; ++i)
            {
                zip_stat st;
                zip_stat_init(&st);

                if (zip_stat_index(zip, i, 0, &st) == -1)
                {
                    std::cerr << "zip_stat_index: Failed to stat file #" << i
                              << " in '" + path + "'" << std::endl;
                    break;
                }

                std::string fPath(Glib::build_filename(extractedPath, st.name));

                if (fPath.back() == '/')
                {
                    g_mkdir_with_parents(fPath.c_str(), 0755);
                }
                else
                {
                    zip_file *file = zip_fopen_index(zip, i, 0);
                    if (!file)
                    {
                        std::cerr << "zip_fopen_index: Failed to open file #" << i
                                  << " (" << st.name << ") in '" + path + "'" << std::endl;
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

                m_SignalExtractorProgress(i + 1, nEntries);
            }
        }

        zip_close(zip);
    }
    else
    {
        std::cerr << "zip_open: Failed to open '" + path + "'" << std::endl;
    }

    return extractedPath;
}
#endif // HAVE_LIBZIP
