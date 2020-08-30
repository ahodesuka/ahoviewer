#pragma once

#include "archive.h"

namespace AhoViewer
{
    class Rar : public Archive
    {
    public:
        Rar(const std::string& path, const std::string& ex_dir);
        ~Rar() override = default;

        bool extract(const std::string& file) const override;
        bool has_valid_files(const FileType t) const override;
        std::vector<std::string> get_entries(const FileType t) const override;

        static constexpr int MagicSize{ 6 };
        static constexpr char Magic[MagicSize]{ 'R', 'a', 'r', '!', 0x1A, 0x07 };
    };
}
