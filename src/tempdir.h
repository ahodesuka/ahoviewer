#pragma once

#include "config.h"

#include <cstring>
#include <glib/gstdio.h>
#include <glibmm.h>
#include <iostream>

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

        std::string make_dir()
        {
            std::string tmpl(Glib::build_filename(m_Path, "XXXXXX"));
            std::string path = g_mkdtemp_full(const_cast<char*>(tmpl.c_str()), 0755);

            return path;
        }
        void remove_dir(const std::string& dir_path)
        {
            // Make sure the directory is in the tempdir
            if (dir_path.compare(0, m_Path.length(), m_Path) == 0)
            {
                for (auto&& i : Glib::Dir(dir_path))
                {
                    std::string path = Glib::build_filename(dir_path, i);
                    if (Glib::file_test(path, Glib::FILE_TEST_IS_DIR))
                        remove_dir(path);
                    else
                        g_unlink(path.c_str());
                }
                g_rmdir(dir_path.c_str());
            }
        }
        std::string get_dir() const { return m_Path; }

    private:
        TempDir()
        {
            // Clean up previous temp directories if they exist
            std::string tmp_path{ Glib::get_tmp_dir() };
#ifdef _WIN32
            // Get around NTFS 260 character path limit
            tmp_path = "\\\\?\\" + tmp_path;
#endif

            for (auto&& dir : Glib::Dir(Glib::get_tmp_dir()))
            {
                std::string filename{ PACKAGE "." };
                // 7 = strlen(".XXXXXX")
                if (dir.compare(0, filename.length(), filename) == 0 &&
                    dir.length() == filename.length() + 7)
                    remove_dir(Glib::build_filename(tmp_path, dir));
            }

            std::string tmpl(Glib::build_filename(tmp_path, PACKAGE ".XXXXXX"));
            m_Path = g_mkdtemp(const_cast<char*>(tmpl.c_str()));
        }
        ~TempDir() { remove_dir(m_Path); }

        std::string m_Path;
    };
}
