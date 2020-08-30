#include "util.h"

#include "settings.h"

#include <date/tz.h>
#include <glibmm/i18n.h>

namespace AhoViewer::Booru
{
    std::istream& operator>>(std::istream& in, Tag& e)
    {
        std::string s;
        in >> s;
        e.tag = s;

        // Should have the type after the space
        if (in.peek() == ' ')
        {
            int type;
            in >> type;

            if (type >= 0 && type <= 6)
                e.type = static_cast<Tag::Type>(type);
        }

        return in;
    }
    std::ostream& operator<<(std::ostream& out, const Tag& e)
    {
        out << e.tag << ' ' << static_cast<int>(e.type);
        return out;
    }

    Tag::operator Gdk::RGBA() const
    {
        switch (type)
        {
        case Type::ARTIST:
        {
            static Gdk::RGBA c{ Settings.get_string("TagArtistColor") };
            return c;
        }
        case Type::CHARACTER:
        {
            static Gdk::RGBA c{ Settings.get_string("TagCharacterColor") };
            return c;
        }
        case Type::COPYRIGHT:
        {
            static Gdk::RGBA c{ Settings.get_string("TagCopyrightColor") };
            return c;
        }
        case Type::METADATA:
        {
            static Gdk::RGBA c{ Settings.get_string("TagMetadataColor") };
            return c;
        }
        default:
            return {};
        }
    }

    std::string format_date_time(const date::sys_seconds t)
    {
        try
        {
            auto my_zone{ date::make_zoned(date::current_zone(), t) };
            return date::format("%c", my_zone);
        }
        // current_zone will throw if no timezone data is available, this will fallback
        // and format dates with in UTC
        catch (const std::runtime_error& e)
        {
            return date::format("%c", t);
        }
    }

    std::string get_rating_string(std::string_view rating)
    {
        if (rating == "s")
            return _("Safe");
        else if (rating == "q")
            return _("Questionable");
        else if (rating == "e")
            return _("Explicit");

        return std::string(rating);
    }
}
