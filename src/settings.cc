#include <glib/gstdio.h>

#include <fstream>
#include <iostream>

#include "settings.h"
using namespace AhoViewer;
using namespace AhoViewer::Booru;

#include "config.h"

SettingsManager AhoViewer::Settings;

SettingsManager::SettingsManager()
  : ConfigPath(Glib::build_filename(Glib::get_user_config_dir(), PACKAGE)),
    ConfigFilePath(Glib::build_filename(ConfigPath, PACKAGE ".cfg")),
    BooruPath(Glib::build_filename(ConfigPath, "booru")),
    FavoriteTagsPath(Glib::build_filename(ConfigPath, "favorite-tags")),
// Defaults {{{
    DefaultBools(
    {
        { "AutoOpenArchive",      true  },
        { "MangaMode",            true  },
        { "RememberLastFile",     true  },
        { "RememberLastSavePath", true  },
        { "SaveThumbnails",       true  },
        { "StartFullscreen",      false },
        { "StoreRecentFiles",     true  },
        { "SmartNavigation",      false },

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
        { "CursorHideDelay",  2   },
        { "TagViewPosition",  560 },
        { "SelectedBooru",    0   },
        { "BooruLimit",       50  },
        { "BooruWidth",       -1  },
    }),
    DefaultSites(
    {
        std::make_tuple("Danbooru",   "https://danbooru.donmai.us",  Site::Type::DANBOORU, "", ""),
        std::make_tuple("Gelbooru",   "http://gelbooru.com",         Site::Type::GELBOORU, "", ""),
        std::make_tuple("Konachan",   "http://konachan.com",         Site::Type::MOEBOORU, "", ""),
        std::make_tuple("yande.re",   "https://yande.re",            Site::Type::MOEBOORU, "", ""),
        std::make_tuple("Safebooru",  "http://safebooru.org",        Site::Type::GELBOORU, "", ""),
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
            "ViewMode",
            {
                { "ToggleMangaMode",     "g" },
                { "AutoFitMode",         "a" },
                { "FitWidthMode",        "w" },
                { "FitHeightMode",       "h" },
                { "ManualZoomMode",      "m" },
            }
        },
        {
            "UserInterface",
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
            "BooruBrowser",
            {
                { "NewTab",              "<Primary>t"        },
                { "SaveImage",           "<Primary>s"        },
                { "SaveImages",          "<Primary><Shift>s" },
                { "ViewPost",            "<Primary><Shift>o" },
                { "CopyImageURL",        "y"                 },
            }
        }
    }),
    DefaultBGColor("#161616")
// }}}
{
    Config.setTabWidth(4); // this is very important
    if (Glib::file_test(ConfigFilePath, Glib::FILE_TEST_EXISTS))
    {
        try
        {
            Config.readFile(ConfigFilePath.c_str());
        }
        catch (const libconfig::ParseException &ex)
        {
            std::cerr << "libconfig::Config.readFile: " << ex.what() << std::endl;
        }
    }

    if (!Glib::file_test(BooruPath, Glib::FILE_TEST_EXISTS))
        g_mkdir_with_parents(BooruPath.c_str(), 0700);

    if (Glib::file_test(FavoriteTagsPath, Glib::FILE_TEST_EXISTS))
    {
        std::ifstream ifs(FavoriteTagsPath);

        if (ifs)
            std::copy(std::istream_iterator<std::string>(ifs),
                      std::istream_iterator<std::string>(),
                      std::inserter(m_FavoriteTags, m_FavoriteTags.begin()));
    }

    load_keybindings();
}

SettingsManager::~SettingsManager()
{
    save_sites();

    try
    {
        Config.writeFile(ConfigFilePath.c_str());
    }
    catch (const libconfig::FileIOException &ex)
    {
        std::cerr << "libconfig::Config.writeFile: " << ex.what() << std::endl;
    }

    if (!m_FavoriteTags.empty())
    {
        std::ofstream ofs(FavoriteTagsPath);

        if (ofs)
            std::copy(m_FavoriteTags.begin(), m_FavoriteTags.end(),
                    std::ostream_iterator<std::string>(ofs, "\n"));
    }
    else if (Glib::file_test(FavoriteTagsPath, Glib::FILE_TEST_EXISTS))
    {
        g_unlink(FavoriteTagsPath.c_str());
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

std::string SettingsManager::get_string(const std::string &key) const
{
    if (Config.exists(key))
        return static_cast<const char*>(Config.lookup(key));

    return "";
}

std::vector<std::shared_ptr<Site>>& SettingsManager::get_sites()
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
            for (size_t i = 0; i < static_cast<size_t>(sites.getLength()); ++i)
            {
                const Setting &s = sites[i];
                std::string username = s.exists("username") ? s["username"] : "",
                            password = s.exists("password") ? s["password"] : "";
                m_Sites.push_back(
                        Site::create(s["name"],
                                     s["url"],
                                     static_cast<Site::Type>(static_cast<int>(s["type"])),
                                     username,
                                     password));
            }

            return m_Sites;
        }
    }
    else
    {
        for (const SiteTuple &s : DefaultSites)
            m_Sites.push_back(Site::create(std::get<0>(s),
                                           std::get<1>(s),
                                           std::get<2>(s),
                                           std::get<3>(s),
                                           std::get<4>(s)));
    }

    return m_Sites;
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

std::string SettingsManager::get_keybinding(const std::string &group, const std::string &name) const
{
    return m_Keybindings.at(group).at(name);
}

/**
 * Clears the first (only) binding that has the same value as value
 * Sets the group and name parameters to those of the binding that was cleared
 * Returns true if it actually cleared a binding
 **/
bool SettingsManager::clear_keybinding(const std::string &value, std::string &group, std::string &name)
{
    for (const std::pair<std::string, std::map<std::string, std::string>> &i : m_Keybindings)
    {
        for (const std::pair<std::string, std::string> &j : i.second)
        {
            if (j.second == value)
            {
                group = i.first;
                name = j.first;

                set_keybinding(group, name, "");

                return true;
            }
        }
    }

    return false;
}

void SettingsManager::set_keybinding(const std::string &group, const std::string &name, const std::string &value)
{
    if (!Config.exists("Keybindings"))
        Config.getRoot().add("Keybindings", Setting::TypeGroup);

    Setting &keys = Config.lookup("Keybindings");

    if (!keys.exists(group))
        keys.add(group, Setting::TypeGroup);

    set(name, value, Setting::TypeString, keys[group.c_str()]);
    m_Keybindings[group][name] = value;
}

std::string SettingsManager::reset_keybinding(const std::string &group, const std::string &name)
{
    if (Config.exists("Keybindings"))
    {
        Setting &keys = Config.lookup("Keybindings");

        if (keys.exists(group) && keys[group.c_str()].exists(name))
            keys[group.c_str()].remove(name);
    }

    return m_Keybindings[group][name] = DefaultKeybindings.at(group).at(name);
}

Gdk::Color SettingsManager::get_background_color() const
{
    if (Config.exists("BackgroundColor"))
        return Gdk::Color(static_cast<const char*>(Config.lookup("BackgroundColor")));

    return DefaultBGColor;
}

void SettingsManager::set_background_color(const Gdk::Color &value)
{
    set("BackgroundColor", value.to_string());
}

Site::Rating SettingsManager::get_booru_max_rating() const
{
    if (Config.exists("BooruMaxRating"))
        return Site::Rating(static_cast<int>(Config.lookup("BooruMaxRating")));

    return DefaultBooruMaxRating;
}

void SettingsManager::set_booru_max_rating(const Site::Rating value)
{
    set("BooruMaxRating", static_cast<int>(value));
}

ImageBox::ZoomMode SettingsManager::get_zoom_mode() const
{
    if (Config.exists("ZoomMode"))
        return ImageBox::ZoomMode(static_cast<const char*>(Config.lookup("ZoomMode"))[0]);

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

void SettingsManager::load_keybindings()
{
    if (Config.exists("Keybindings"))
    {
        Setting &keys = Config.lookup("Keybindings");

        for (const std::pair<std::string, std::map<std::string, std::string>> &i : DefaultKeybindings)
        {
            if (keys.exists(i.first))
            {
                for (const std::pair<std::string, std::string> &j : i.second)
                {
                    if (keys[i.first.c_str()].exists(j.first))
                    {
                        m_Keybindings[i.first][j.first] = std::string(keys[i.first.c_str()][j.first.c_str()].c_str());
                    }
                    else
                    {
                        m_Keybindings[i.first][j.first] = DefaultKeybindings.at(i.first).at(j.first);
                    }
                }
            }
            else
            {
                m_Keybindings[i.first] = DefaultKeybindings.at(i.first);
            }
        }
    }
    else
    {
        m_Keybindings = DefaultKeybindings;
    }
}

void SettingsManager::save_sites()
{
    remove("Sites");
    Setting &sites = Config.getRoot().add("Sites", Setting::TypeList);

    for (const std::shared_ptr<Site> &s : m_Sites)
    {
        Setting &site = sites.add(Setting::TypeGroup);
        set("name", s->get_name(), Setting::TypeString, site);
        set("url", s->get_url(), Setting::TypeString, site);
        set("type", static_cast<int>(s->get_type()), Setting::TypeInt, site);
        set("username", s->get_username(), Setting::TypeString, site);
#ifndef HAVE_LIBSECRET
        set("password", s->get_password(), Setting::TypeString, site);
#endif // !HAVE_LIBSECRET
        s->cleanup_cookie();
    }
}
