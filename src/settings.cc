#include <iostream>

#include "settings.h"
using namespace AhoViewer;

#include "config.h"

SettingsManager AhoViewer::Settings;

SettingsManager::SettingsManager()
  : KeyFile(),
    Path(Glib::build_filename(Glib::get_user_config_dir(), PACKAGE, PACKAGE ".conf")),
    BooruPath(Glib::build_filename(Glib::get_user_config_dir(), PACKAGE, "booru")),
// Defaults {{{
    DefaultBools(
    {
        { "AutoOpenArchive",      true  },
        { "MangaMode",            true  },
        { "RememberLastFile",     true  },
        { "SaveThumbnails",       true  },
        { "StartFullscreen",      false },
        { "StoreRecentFiles",     true  },

        { "BooruBrowserVisible",  true  },
        { "MenuBarVisible",       true  },
        { "ScrollbarsVisible",    true  },
        { "StatusBarVisible",     true  },
        { "ThumbnailBarVisible",  false },
        { "HideAll",              false },
        { "HideAllFullscreen",    true  },
    }),
    DefaultInts(
    {
        { "ArchiveIndex",     -1  },
        { "CacheSize",        2   },
        { "SlideshowDelay",   5   },
        { "TagViewPosition",  560 },
        { "SelectedBooru",    0   },
        { "BooruLimit",       50  },
        { "BooruWidth",       -1  },
    }),
    DefaultSites(
    {
        std::make_shared<Booru::Site>("Gelbooru",   "http://gelbooru.com",        Booru::Site::Type::GELBOORU),
        std::make_shared<Booru::Site>("Danbooru",   "http://danbooru.donmai.us",  Booru::Site::Type::DANBOORU),
        std::make_shared<Booru::Site>("Konachan",   "http://konachan.com",        Booru::Site::Type::DANBOORU),
        std::make_shared<Booru::Site>("yande.re",   "https://yande.re",           Booru::Site::Type::DANBOORU),
        std::make_shared<Booru::Site>("Safebooru",  "http://safebooru.org",       Booru::Site::Type::GELBOORU),
    }),
    DefaultKeybindings(
    {
        {
            "File",
            {
                { "OpenFile",            "<Primary>o" },
                { "Preferences",         "p"          },
                { "Close",               "<Primary>w" },
                { "Quit",                "<Primary>q" },
            }
        },
        {
            "View Mode",
            {
                { "ToggleMangaMode",     "g" },
                { "AutoFitMode",         "a" },
                { "FitWidthMode",        "w" },
                { "FitHeightMode",       "h" },
                { "ManualZoomMode",      "m" },
            }
        },
        {
            "User Interface",
            {
                { "ToggleFullscreen",    "f"          },
                { "ToggleMenuBar",       "<Primary>m" },
                { "ToggleStatusBar",     "<Primary>b" },
                { "ToggleScrollbars",    "<Primary>l" },
                { "ToggleThumbnailBar",  "t"          },
                { "ToggleBooruBrowser",  "b"          },
                { "ToggleHideAll",       "i"          },
            }
        },
        {
            "Zoom",
            {
                { "ZoomIn",              "<Primary>equal" },
                { "ZoomOut",             "<Primary>minus" },
                { "ResetZoom",           "<Primary>0"     },
            }
        },
        {
            "Navigation",
            {
                { "NextImage",           "Page_Down" },
                { "PreviousImage",       "Page_Up"   },
                { "FirstImage",          "Home"      },
                { "LastImage",           "End"       },
                { "ToggleSlideshow",     "s"         },
            }
        },
        {
            "Scroll",
            {
                { "ScrollUp",            "Up"    },
                { "ScrollDown",          "Down"  },
                { "ScrollLeft",          "Left"  },
                { "ScrollRight",         "Right" },
            }
        },
        {
            "Booru Browser",
            {
                { "NewTab",              "<Primary>t" },
                { "SaveImages",          "<Primary>s" },
            }
        }
    }),
    DefaultBGColor("#161616")
// }}}
{
    if (Glib::file_test(Path, Glib::FILE_TEST_EXISTS))
    {
        try
        {
            KeyFile.load_from_file(Path);
        }
        catch (const Glib::KeyFileError &ex)
        {
            std::cerr << "Glib::KeyFile::load_from_file: " << ex.what() << std::endl;
        }
    }
}

SettingsManager::~SettingsManager()
{
    Glib::file_set_contents(Path, KeyFile.to_data());
}

bool SettingsManager::get_bool(const std::string &key) const
{
    if (KeyFile.has_group(PACKAGE) && KeyFile.has_key(PACKAGE, key))
        return KeyFile.get_boolean(PACKAGE, key);

    return DefaultBools.at(key);
}

void SettingsManager::set_bool(const std::string &key, const bool value)
{
    KeyFile.set_boolean(PACKAGE, key, value);
}

int SettingsManager::get_int(const std::string &key) const
{
    if (KeyFile.has_group(PACKAGE) && KeyFile.has_key(PACKAGE, key))
        return KeyFile.get_integer(PACKAGE, key);

    return DefaultInts.at(key);
}

void SettingsManager::set_int(const std::string &key, const int value)
{
    KeyFile.set_integer(PACKAGE, key, value);
}

std::vector<std::shared_ptr<Booru::Site>> SettingsManager::get_sites()
{
    if (m_Sites.size())
    {
        return m_Sites;
    }
    else if (KeyFile.has_group("sites"))
    {
        std::vector<std::string> keys = KeyFile.get_keys("sites");

        if (keys.size())
        {
            for (const std::string &key : keys)
            {
                std::vector<std::string> s = KeyFile.get_string_list("sites", key);
                m_Sites.push_back(std::make_shared<Booru::Site>(s[0], s[1], Booru::Site::string_to_type(s[2])));
            }

            return m_Sites;
        }
    }

    return DefaultSites;
}

bool SettingsManager::get_geometry(int &x, int &y, int &w, int &h) const
{
    if (KeyFile.has_group("geometry") && KeyFile.has_key("geometry", "x") &&
                                         KeyFile.has_key("geometry", "y") &&
                                         KeyFile.has_key("geometry", "w") &&
                                         KeyFile.has_key("geometry", "h"))
    {
        x = KeyFile.get_integer("geometry", "x");
        y = KeyFile.get_integer("geometry", "y");
        w = KeyFile.get_integer("geometry", "w");
        h = KeyFile.get_integer("geometry", "h");

        return true;
    }

    return false;
}

void SettingsManager::set_geometry(const int x, const int y, const int w, const int h)
{
    KeyFile.set_integer("geometry", "x", x);
    KeyFile.set_integer("geometry", "y", y);
    KeyFile.set_integer("geometry", "w", w);
    KeyFile.set_integer("geometry", "h", h);
}

bool SettingsManager::get_last_open_file(std::string &path) const
{
    if (KeyFile.has_key(PACKAGE, "LastOpenFile"))
    {
        path = KeyFile.get_string(PACKAGE, "LastOpenFile");
        return true;
    }

    return false;
}

void SettingsManager::set_last_open_file(const std::string &path)
{
    KeyFile.set_string(PACKAGE, "LastOpenFile", path);
}

std::string SettingsManager::get_keybinding(const std::string &group, const std::string &name) const
{
    return DefaultKeybindings.at(group).at(name);
}

void SettingsManager::set_keybinding(const std::string &group, const std::string &name, const std::string &value)
{
    KeyFile.set_string(group, name, value);
}

Gdk::Color SettingsManager::get_background_color() const
{
    if (KeyFile.has_group(PACKAGE) && KeyFile.has_key(PACKAGE, "BackgroundColor"))
        return Gdk::Color(KeyFile.get_string(PACKAGE, "BackgroundColor"));

    return DefaultBGColor;
}

void SettingsManager::set_background_color(const Gdk::Color &value)
{
    KeyFile.set_string(PACKAGE, "BackgroundColor", value.to_string());
}

Booru::Site::Rating SettingsManager::get_booru_max_rating() const
{
    if (KeyFile.has_group(PACKAGE) && KeyFile.has_key(PACKAGE, "BooruMaxRating"))
        return Booru::Site::Rating(KeyFile.get_integer(PACKAGE, "BooruMaxRating"));

    return DefaultBooruMaxRating;
}

void SettingsManager::set_booru_max_rating(const Booru::Site::Rating value)
{
    KeyFile.set_integer(PACKAGE, "BooruMaxRating", static_cast<int>(value));
}

ImageBox::ZoomMode SettingsManager::get_zoom_mode() const
{
    if (KeyFile.has_group(PACKAGE) && KeyFile.has_key(PACKAGE, "ZoomMode"))
        return ImageBox::ZoomMode(KeyFile.get_string(PACKAGE, "ZoomMode")[0]);

    return DefaultZoomMode;
}

void SettingsManager::set_zoom_mode(const ImageBox::ZoomMode value)
{
    KeyFile.set_string(PACKAGE, "ZoomMode", std::string(1, static_cast<char>(value)));
}

void SettingsManager::remove_key(const std::string &key)
{
    if (KeyFile.has_key(PACKAGE, key))
        KeyFile.remove_key(PACKAGE, key);
}
