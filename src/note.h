#pragma once

#include <string>

namespace AhoViewer
{
    struct Note
    {
        Note(const std::string &body, const int w, const int h,
                                      const int x, const int y)
          : body{ body },
            w{ w },
            h{ h },
            x{ x },
            y{ y }
        { }
        virtual ~Note() = default;

        std::string body;
        int w, h, x, y;
    };
}
