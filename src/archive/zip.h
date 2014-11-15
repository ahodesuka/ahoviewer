#ifndef _ZIP_H_
#define _ZIP_H_

#include <zip.h>

#include "archive.h"

namespace AhoViewer
{
    class Zip : public Archive::Extractor
    {
    public:
        virtual std::string extract(const std::string&) const;

        static int const MagicSize = 4;
        static const char Magic[MagicSize];
    private:
        typedef struct zip      zip_archive;
        typedef struct zip_stat zip_stat;
        typedef struct zip_file zip_file;
    };
}

#endif /* _ZIP_H_ */
