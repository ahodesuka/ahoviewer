#ifndef _RAR_H_
#define _RAR_H_

#if defined(__linux__) || defined(_APPLE) || defined(__MINGW32__)
  #define _UNIX
#endif

#if defined(HAVE_LIBUNRAR_DLL_HPP)
#include <libunrar/dll.hpp>
#elif defined(HAVE_UNRAR_DLL_HPP)
#include <unrar/dll.hpp>
#endif

#include "archive.h"

namespace AhoViewer
{
    class Rar : public Archive::Extractor
    {
    public:
        virtual std::string extract(const std::string&, const std::shared_ptr<Archive>&) const;

        static int const MagicSize = 6;
        static const char Magic[MagicSize];
    };
}

#endif /* _RAR_H_ */
