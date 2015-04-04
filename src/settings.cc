#include <iostream>

#include "settings.h"
using namespace AhoViewer;

#include "config.h"

SettingsManager AhoViewer::Settings;

SettingsManager::SettingsManager()
  : Config(),
    Path(Glib::build_filename(Glib::get_user_config_dir(), PACKAGE, PACKAGE ".cfg")),
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
                { "NewTab",              "<Primary>t"        },
                { "SaveImage",           "<Primary><Shift>s" },
                { "SaveImages",          "<Primary>s"        },
            }
        }
    }),
    DefaultBGColor("#161616")
// }}}
{
    Config.setTabWidth(4); // this is very important
    if (Glib::file_test(Path, Glib::FILE_TEST_EXISTS))
    {
        try
        {
            Config.readFile(Path.c_str());
        }
        catch (const libconfig::ParseException &ex)
        {
            std::cerr << "libconfig::Config.readFile: " << ex.what() << std::endl;
        }
    }
}

SettingsManager::~SettingsManager()
{
    try
    {
        Config.writeFile(Path.c_str());
    }
    catch (const libconfig::FileIOException &ex)
    {
        std::cerr << "libconfig::Config.writeFile: " << ex.what() << std::endl;
    }
}

bool SettingsManager::get_bool(const std::string &key) const
{
    if (Config.exists(key))
        return Config.lookup(key);

    return DefaultBools.at(key);
}

int SettingsManager::get_int(const std::string &key) const
{
    if (Config.exists(key))
        return Config.lookup(key);

    return DefaultInts.at(key);
}

std::vector<std::shared_ptr<Booru::Site>> SettingsManager::get_sites()
{
    if (m_Sites.size())
    {
        return m_Sites;
    }
    else if (Config.exists("Sites"))
    {
        const Setting &sites = Config.lookup("Sites");
        if (sites.getLength() > 0)
        {
            for (size_t i = 0; i < (size_t)sites.getLength(); ++i)
            {
                const Setting &s = sites[i];
                m_Sites.push_back(std::make_shared<Booru::Site>(s["name"], s["url"],
                            Booru::Site::string_to_type(s["type"])));
            }

            return m_Sites;
        }
    }

    return DefaultSites;
}

bool SettingsManager::get_geometry(int &x, int &y, int &w, int &h) const
{
    if (Config.lookupValue("Geometry.x", x) && Config.lookupValue("Geometry.y", y) &&
        Config.lookupValue("Geometry.w", w) && Config.lookupValue("Geometry.h", h))
    {
        return true;
    }

    return false;
}

void SettingsManager::set_geometry(const int x, const int y, const int w, const int h)
{
    if (!Config.exists("Geometry"))
        Config.getRoot().add("Geometry", Setting::TypeGroup);

    Setting &geo = Config.lookup("Geometry");

    set("x", x, Setting::TypeInt, geo);
    set("y", y, Setting::TypeInt, geo);
    set("w", w, Setting::TypeInt, geo);
    set("h", h, Setting::TypeInt, geo);
}

bool SettingsManager::get_last_open_file(std::string &path) const
{
    if (Config.exists("LastOpenFile"))
    {
        path = (const char*)Config.lookup("LastOpenFile");
        return true;
    }

    return false;
}

std::string SettingsManager::get_keybinding(const std::string &group, const std::string &name) const
{
    return DefaultKeybindings.at(group).at(name);
}

void SettingsManager::set_keybinding(const std::string &group, const std::string &name, const std::string &value)
{
    if (!Config.exists("Keybindings"))
        Config.getRoot().add("Keybindings", Setting::TypeGroup);

    Setting &keys = Config.lookup("Keybindings");

    if (!keys.exists(group))
        keys.add(group, Setting::TypeGroup);

    set(name, value, Setting::TypeString, keys[group]);
}

Gdk::Color SettingsManager::get_background_color() const
{
    if (Config.exists("BackgroundColor"))
        return Gdk::Color((const char*)Config.lookup("BackgroundColor"));

    return DefaultBGColor;
}

void SettingsManager::set_background_color(const Gdk::Color &value)
{
    set("BackgroundColor", value.to_string());
}

Booru::Site::Rating SettingsManager::get_booru_max_rating() const
{
    if (Config.exists("BooruMaxRating"))
        return Booru::Site::Rating((int)Config.lookup("BooruMaxRating"));

    return DefaultBooruMaxRating;
}

void SettingsManager::set_booru_max_rating(const Booru::Site::Rating value)
{
    set("BooruMaxRating", static_cast<int>(value));
}

ImageBox::ZoomMode SettingsManager::get_zoom_mode() const
{
    if (Config.exists("ZoomMode"))
        return ImageBox::ZoomMode(((const char*)Config.lookup("ZoomMode"))[0]);

    return DefaultZoomMode;
}

void SettingsManager::set_zoom_mode(const ImageBox::ZoomMode value)
{
    set("ZoomMode", std::string(1, static_cast<char>(value)));
}

void SettingsManager::remove(const std::string &key)
{
    if (Config.exists(key))
        Config.getRoot().remove(key);
}
