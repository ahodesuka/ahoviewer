#include "util.h"

#include "settings.h"

namespace AhoViewer::Booru
{
    std::istream& operator>>(std::istream& in, Tag& e)
    {
        std::string s;
        in >> s;
        e.tag = s;

        auto p{ in.peek() };
        // Should have the type after the space
        if (p == ' ')
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
}
