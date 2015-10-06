#ifndef _ZIP_H_
#define _ZIP_H_

#include "archive.h"

namespace AhoViewer
{
    class Zip : public Archive
    {
    public:
        Zip(const std::string &path, const std::string &exDir, const std::string &parentDir);
        virtual ~Zip() = default;

        virtual void extract();
        virtual bool has_valid_files(const FileType t) const;
        virtual size_t get_n_valid_files(const FileType t) const;

        static const int MagicSize = 4;
        static const char Magic[MagicSize];
    };
}

#endif /* _ZIP_H_ */
