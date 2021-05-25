#pragma once

#include <date/date.h>
#include <gdkmm.h>
#include <glibmm.h>
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
        ScrollPos(const double h, const double v, const ZoomMode zoom)
            : h{ h },
              v{ v },
              zoom{ zoom }
        {
        }
        double h, v;
        ZoomMode zoom;
    };

    namespace Booru
    {
        static const int IconViewItemPadding{ 2 };
        struct PostInfo
        {
            PostInfo(std::string date, std::string source, std::string rating, std::string score)
                : date{ std::move(date) },
                  source{ std::move(source) },
                  rating{ std::move(rating) },
                  score{ std::move(score) }
            {
            }

            const std::string date, source, rating, score;
        };
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
            DANBOORU    = 4,
            PLUGIN      = 5,
        };
        struct Tag
        {
            // These values determine the order when sorting by type
            enum class Type
            {
                ARTIST     = 0,
                CHARACTER  = 1,
                COPYRIGHT  = 2,
                METADATA   = 3,
                GENERAL    = 4,
                DEPRECATED = 5,
                UNKNOWN    = 6,
            };

            Tag() = default;
            Tag(std::string tag, const Type type = Type::UNKNOWN)
                : tag{ std::move(tag) },
                  type{ type }
            {
            }
            Tag(const Tag& orig) = default;

            friend std::istream& operator>>(std::istream& in, Tag& e);
            friend std::ostream& operator<<(std::ostream& out, const Tag& e);

            operator std::string() const { return tag; }
            operator Glib::ustring() const { return tag; }
            operator Gdk::RGBA() const;

            inline bool operator==(const Tag& rhs) const { return tag == rhs.tag; }
            inline bool operator!=(const Tag& rhs) const { return !(*this == rhs); }
            inline bool operator<(const Tag& rhs) const { return tag.compare(rhs.tag) < 0; }
            inline bool operator>(const Tag& rhs) const { return tag.compare(rhs.tag) > 0; }
            inline bool operator<=(const Tag& rhs) const { return tag.compare(rhs.tag) <= 0; }
            inline bool operator>=(const Tag& rhs) const { return tag.compare(rhs.tag) >= 0; }

            std::string tag;
            mutable Type type{ Type::UNKNOWN };
        };
        enum class TagViewOrder
        {
            TYPE = 0,
            TAG  = 1,
        };

        using PostDataTuple = std::
            //    image_url    thumb_url    post_url     notes_url
            tuple<std::string, std::string, std::string, std::string, std::vector<Tag>, PostInfo>;

        std::string format_date_time(const date::sys_seconds t);
        std::string get_rating_string(std::string_view rating);
    }

    namespace Util
    {
        std::wstring utf8_to_utf16(const std::string& s);
        std::string utf16_to_utf8(const std::wstring& s);
        std::string null_check_string(const gchar* s);
    }
}
