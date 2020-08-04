#ifndef _UTIL_H_
#define _UTIL_H_

#include <string>
#include <utility>

namespace AhoViewer
{
    enum class ZoomMode : char
    {
        AUTO_FIT   = 'A',
        FIT_WIDTH  = 'W',
        FIT_HEIGHT = 'H',
        MANUAL     = 'M',
    };
    struct Note
    {
        Note(std::string body, const int w, const int h, const int x, const int y)
            : body{ std::move(body) },
              w{ w },
              h{ h },
              x{ x },
              y{ y }
        {
        }
        std::string body;
        int w, h, x, y;
    };
    struct ScrollPos
    {
        ScrollPos(double h, double v, ZoomMode zoom) : h{ h }, v{ v }, zoom{ zoom } { }
        double h, v;
        ZoomMode zoom;
    };

    namespace Booru
    {
        enum class Rating
        {
            SAFE         = 0,
            QUESTIONABLE = 1,
            EXPLICIT     = 2,
        };
        enum class Type
        {
            UNKNOWN     = -1,
            DANBOORU_V2 = 0,
            GELBOORU    = 1,
            MOEBOORU    = 2,
            SHIMMIE     = 3,
            DANBOORU    = 4,
        };
    }
}

#endif /* _UTIL_H_ */
