#ifndef _TEMPDIR_H_
#define _TEMPDIR_H_

#include <glibmm.h>
#include <glib/gstdio.h>

#include <cstring>
#include <iostream>

#include "config.h"

namespace AhoViewer
{
    class TempDir
    {
    public:
        static TempDir& get_instance()
        {
            static TempDir i;
            return i;
        }

        std::string make_dir(std::string dirPath = "")
        {
            std::string path;

            if (dirPath == "")
            {
                std::string tmpl(Glib::build_filename(m_Path, "XXXXXX"));
                path = g_mkdtemp_full(const_cast<char*>(tmpl.c_str()), 0755);
            }
            else
            {
                path = Glib::build_filename(m_Path, dirPath);
                // Loop until we have a unique directory name
                for (size_t i = 1; Glib::file_test(path, Glib::FILE_TEST_EXISTS); ++i)
                    path = Glib::build_filename(m_Path, dirPath + "-" + std::to_string(i));

                if (g_mkdir_with_parents(path.c_str(), 0755) == -1)
                {
                    std::cerr << "g_mkdir_with_parents: Failed to create '" << path << "'" << std::endl;
                    return "";
                }
            }

            return path;
        }
        void remove_dir(const std::string &dirPath)
        {
            // Make sure the directory is in the tempdir
            if (dirPath.compare(0, m_Path.length(), m_Path) == 0)
            {
                Glib::Dir dir(dirPath);
                for (Glib::DirIterator i = dir.begin(); i != dir.end(); ++i)
                {
                    std::string path = Glib::build_filename(dirPath, *i);
                    if (Glib::file_test(path, Glib::FILE_TEST_IS_DIR))
                        remove_dir(path);
                    else
                        g_unlink(path.c_str());
                }
                g_rmdir(dirPath.c_str());
            }
        }
        std::string get_dir() const
        {
            return m_Path;
        }
    private:
        TempDir()
        {
            // Clean up previous temp directories if they exist
            Glib::Dir dir(Glib::get_tmp_dir());
            std::vector<std::string> dirs(dir.begin(), dir.end());

            for (auto i = dirs.begin(); i != dirs.end(); ++i)
            {
                // 7 = strlen(".XXXXXX")
                if (i->find(PACKAGE ".") != std::string::npos
                 && i->length() == strlen(PACKAGE) + 7)
                    remove_dir(Glib::build_filename(Glib::get_tmp_dir(), *i));
            }

            std::string tmpl(Glib::build_filename(Glib::get_tmp_dir(), PACKAGE ".XXXXXX"));
            m_Path = g_mkdtemp(const_cast<char*>(tmpl.c_str()));
        }
        ~TempDir() { remove_dir(m_Path); }

        std::string m_Path;
    };
}

#endif /* _TEMPDIR_H_ */
