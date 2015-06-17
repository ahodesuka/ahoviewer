#ifndef _ZIP_H_
#define _ZIP_H_

#include <zip.h>

#include "archive.h"

namespace AhoViewer
{
    class Zip : public Archive::Extractor
    {
    public:
        virtual std::string extract(const std::string&, const std::shared_ptr<Archive>&) const;

        static int const MagicSize = 4;
        static const char Magic[MagicSize];
    };
}

#endif /* _ZIP_H_ */
