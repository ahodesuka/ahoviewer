#ifndef _RAR_H_
#define _RAR_H_

#include "archive.h"

namespace AhoViewer
{
    class Rar : public Archive
    {
    public:
        Rar(const std::string &path, const std::string &exDir, const std::string &parentDir);
        virtual ~Rar() = default;

        virtual void extract();
        virtual bool has_valid_files(const FileType t) const;
        virtual size_t get_n_valid_files(const FileType t = static_cast<FileType>(IMAGES | ARCHIVES)) const;

        static const int MagicSize = 6;
        static const char Magic[MagicSize];
    };
}

#endif /* _RAR_H_ */
