#pragma once

namespace AhoViewer
{
    enum class ZoomMode : char
    {
        AUTO_FIT   = 'A',
        FIT_WIDTH  = 'W',
        FIT_HEIGHT = 'H',
        MANUAL     = 'M',
    };
    struct ScrollPos
    {
        int h, v;
        ZoomMode zoom;
        ScrollPos(double h, double v, ZoomMode zoom) :
            h(h), v(v), zoom(zoom) { }
    };

    namespace Booru
    {
        enum class Rating
        {
            SAFE = 0,
            QUESTIONABLE,
            EXPLICIT,
        };
        enum class Type
        {
            UNKNOWN = -1,
            DANBOORU_V2,
            GELBOORU,
            MOEBOORU,
            SHIMMIE,
            DANBOORU,
        };
    }
}
