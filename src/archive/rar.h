#ifndef _RAR_H_
#define _RAR_H_

#include <libunrar/raros.hpp>
#include <libunrar/dll.hpp>

#include "archive.h"

namespace AhoViewer
{
    class Rar : public Archive::Extractor
    {
    public:
        virtual std::string extract(const std::string&) const;

        static int const MagicSize = 6;
        static const char Magic[MagicSize];
    };
}

#endif /* _RAR_H_ */
