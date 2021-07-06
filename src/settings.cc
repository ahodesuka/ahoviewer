#include "settings.h"
using namespace AhoViewer;
using namespace AhoViewer::Booru;

#include "application.h"
#include "booru/site.h"
#include "config.h"
#include "imagebox.h"

#include <fstream>
#include <glib/gstdio.h>
#include <iostream>

SettingsManager AhoViewer::Settings;

SettingsManager::SettingsManager()
    : m_ConfigPath(Glib::build_filename(Glib::get_user_config_dir(), PACKAGE)),
      m_ConfigFilePath(Glib::build_filename(m_ConfigPath, PACKAGE ".cfg")),
      m_BooruPath(Glib::build_filename(m_ConfigPath, "booru")),
      m_FavoriteTagsPath(Glib::build_filename(m_ConfigPath, "favorite-tags")),
      // Defaults {{{
      m_DefaultBools({ { "AutoOpenArchive", true },    { "MangaMode", true },
                       { "RememberLastFile", true },   { "RememberLastSavePath", true },
                       { "SaveImageTags", false },     { "SaveThumbnails", true },
                       { "StartFullscreen", false },   { "StoreRecentFiles", true },
                       { "SmartNavigation", false },   { "BooruBrowserVisible", true },
                       { "MenuBarVisible", true },     { "ScrollbarsVisible", true },
                       { "StatusBarVisible", true },   { "ThumbnailBarVisible", false },
                       { "HideAll", false },           { "HideAllFullscreen", true },
                       { "RememberWindowSize", true }, { "RememberWindowPos", true },
                       { "ShowTagTypeHeaders", true }, { "AutoHideInfoBox", true },
                       { "AskDeleteConfirm", true },   { "UseRGBAVisual", false } }),
      m_DefaultInts({ { "ArchiveIndex", -1 },
                      { "CacheSize", 2 },
                      { "SlideshowDelay", 5 },
                      { "CursorHideDelay", 2 },
                      { "TagViewPosition", -1 },
                      { "SelectedBooru", 0 },
                      { "BooruLimit", 50 },
                      { "BooruWidth", -1 },
                      { "Volume", 50 },
                      { "ScrollPosH", -1 },
                      { "ScrollPosV", -1 },
                      { "YandereTagsVersion", 0 },
                      { "KonachanTagsVersion", 0 } }),
      m_DefaultStrings({
          { "TitleFormat", "[%i / %c] %f - %p" },
          { "AudioSink", "fakesink" },
          { "TagArtistColor", "#A00" },
          { "TagCharacterColor", "#0A0" },
          { "TagCopyrightColor", "#A0A" },
          { "TagMetadataColor", "#F80" },
      }),
      m_DefaultSites({
          std::make_tuple("Danbooru", "https://danbooru.donmai.us", Type::DANBOORU_V2),
          std::make_tuple("Gelbooru", "https://gelbooru.com", Type::GELBOORU),
          std::make_tuple("Konachan", "https://konachan.com", Type::MOEBOORU),
          std::make_tuple("yande.re", "https://yande.re", Type::MOEBOORU),
          std::make_tuple("Safebooru", "https://safebooru.org", Type::GELBOORU),
      }),
      m_DefaultKeybindings({
          { "File",
            {
                { "OpenFile", "<Primary>o" },
                { "DeleteImage", "<Shift>Delete" },
                { "Preferences", "p" },
                { "Close", "<Primary>w" },
                { "Quit", "<Primary>q" },
            } },
          { "ViewMode",
            {
                { "ToggleMangaMode", "g" },
                { "AutoFitMode", "a" },
                { "FitWidthMode", "w" },
                { "FitHeightMode", "h" },
                { "ManualZoomMode", "m" },
            } },
          { "UserInterface",
            {
                { "ToggleFullscreen", "f" },
                { "ToggleMenuBar", "<Primary>m" },
                { "ToggleStatusBar", "<Primary>b" },
                { "ToggleScrollbars", "<Primary>l" },
                { "ToggleThumbnailBar", "t" },
                { "ToggleBooruBrowser", "b" },
                { "ToggleHideAll", "i" },
            } },
          { "Zoom",
            {
                { "ZoomIn", "<Primary>equal" },
                { "ZoomOut", "<Primary>minus" },
                { "ResetZoom", "<Primary>0" },
            } },
          { "Navigation",
            {
                { "NextImage", "Page_Down" },
                { "PreviousImage", "Page_Up" },
                { "FirstImage", "Home" },
                { "LastImage", "End" },
                { "ToggleSlideshow", "s" },
            } },
          { "Scroll",
            {
                { "ScrollUp", "Up" },
                { "ScrollDown", "Down" },
                { "ScrollLeft", "Left" },
                { "ScrollRight", "Right" },
            } },
          { "BooruBrowser",
            {
                { "NewTab", "<Primary>t" },
                { "SaveImage", "<Shift>s" },
                { "SaveImageAs", "<Primary>s" },
                { "SaveImages", "<Primary><Shift>s" },
                { "ViewPost", "<Primary><Shift>o" },
                { "CopyImageURL", "y" },
                { "CopyImageData", "<Primary><Shift>y" },
                { "CopyPostURL", "<Primary>y" },
            } },
#ifdef HAVE_LIBPEAS
          { "Plugins",
            {

            } },
#endif // HAVE_LIBPEAS
      })
// }}}
{
    if (Glib::file_test(m_ConfigFilePath, Glib::FILE_TEST_EXISTS))
    {
        try
        {
            m_Config.readFile(m_ConfigFilePath.c_str());
        }
        catch (const libconfig::ParseException& ex)
        {
            std::cerr << "libconfig::Config.readFile: " << ex.what() << std::endl;
        }
    }

    if (!Glib::file_test(m_BooruPath, Glib::FILE_TEST_EXISTS))
        g_mkdir_with_parents(m_BooruPath.c_str(), 0700);

    if (Glib::file_test(m_FavoriteTagsPath, Glib::FILE_TEST_EXISTS))
    {
        std::ifstream ifs(m_FavoriteTagsPath);
        if (ifs)
            std::copy(std::istream_iterator<Booru::Tag>{ ifs },
                      std::istream_iterator<Booru::Tag>{},
                      std::back_inserter(m_FavoriteTags));
    }
}

SettingsManager::~SettingsManager()
{
    save_sites();

    try
    {
        m_Config.writeFile(m_ConfigFilePath.c_str());
    }
    catch (const libconfig::FileIOException& ex)
    {
        std::cerr << "libconfig::Config.writeFile: " << ex.what() << std::endl;
    }

    if (!m_FavoriteTags.empty())
    {
        std::ofstream ofs(m_FavoriteTagsPath);

        if (ofs)
            std::copy(m_FavoriteTags.begin(),
                      m_FavoriteTags.end(),
                      std::ostream_iterator<Tag>(ofs, "\n"));
    }
    else if (Glib::file_test(m_FavoriteTagsPath, Glib::FILE_TEST_EXISTS))
    {
        g_unlink(m_FavoriteTagsPath.c_str());
    }
}

bool SettingsManager::get_bool(const std::string& key) const
{
    if (m_Config.exists(key))
        return m_Config.lookup(key);

    return m_DefaultBools.at(key);
}

int SettingsManager::get_int(const std::string& key) const
{
    if (m_Config.exists(key))
        return m_Config.lookup(key);

    return m_DefaultInts.at(key);
}

std::string SettingsManager::get_string(const std::string& key) const
{
    if (m_Config.exists(key))
        return static_cast<const char*>(m_Config.lookup(key));
    else if (m_DefaultStrings.find(key) != m_DefaultStrings.end())
        return m_DefaultStrings.at(key);

    return "";
}

std::vector<std::shared_ptr<Site>>& SettingsManager::get_sites()
{
    if (!m_Sites.empty())
    {
        return m_Sites;
    }
    else if (m_Config.exists("Sites"))
    {
        const Setting& sites = m_Config.lookup("Sites");
        if (sites.getLength() > 0)
        {
            for (size_t i = 0; i < static_cast<size_t>(sites.getLength()); ++i)
            {
                const Setting& s{ sites[i] };
                std::string name{ s.exists("name") ? s["name"] : "" },
                    url{ s.exists("url") ? s["url"] : "" },
                    username{ s.exists("username") ? s["username"] : "" },
                    password{ s.exists("password") ? s["password"] : "" },
                    plugin_name{ s.exists("plugin_name") ? s["plugin_name"] : "" };
                Type type{ static_cast<Type>(static_cast<int>(s["type"])) };
                bool use_samples{ false };
                s.lookupValue("use_samples", use_samples);

                auto site{ Site::create(name, url, type, username, password, use_samples) };

                // unlikely
                if (!site)
                    continue;

                if (type == Type::PLUGIN)
                {
#ifdef HAVE_LIBPEAS
                    const auto& plugins{
                        Application::get_default()->get_plugin_manager().get_site_plugins()
                    };
                    auto it{ std::find_if(
                        plugins.cbegin(), plugins.cend(), [&plugin_name](const auto& p) {
                            return p->get_name() == plugin_name;
                        }) };

                    if (it != plugins.cend())
                    {
                        site->set_plugin(*it);
                    }
                    else
                    {
                        std::cerr << "Previously saved site '" << name
                                  << "' uses a plugin that is no longer installed" << std::endl;
                        m_DisabledSites.emplace_back(
                            name, url, username, password, type, use_samples, plugin_name);
                        continue;
                    }
#else  // !HAVE_LIBPEAS
                    std::cerr << "Previously saved site '" << name
                              << "' uses a plugin, but plugin support was not enabled" << std::endl;
                    continue;
#endif // !HAVE_LIBPEAS
                }

                m_Sites.push_back(std::move(site));
            }

            return m_Sites;
        }
    }

    for (const auto& s : m_DefaultSites)
        m_Sites.push_back(Site::create(std::get<0>(s), std::get<1>(s), std::get<2>(s)));

    return m_Sites;
}

bool SettingsManager::get_geometry(int& x, int& y, int& w, int& h) const
{
    return m_Config.lookupValue("Geometry.x", x) && m_Config.lookupValue("Geometry.y", y) &&
           m_Config.lookupValue("Geometry.w", w) && m_Config.lookupValue("Geometry.h", h);
}

void SettingsManager::set_geometry(const int x, const int y, const int w, const int h)
{
    if (!m_Config.exists("Geometry"))
        m_Config.getRoot().add("Geometry", Setting::TypeGroup);

    Setting& geo{ m_Config.lookup("Geometry") };

    set("x", x, Setting::TypeInt, geo);
    set("y", y, Setting::TypeInt, geo);
    set("w", w, Setting::TypeInt, geo);
    set("h", h, Setting::TypeInt, geo);
}

std::string SettingsManager::get_keybinding(const std::string& group, const std::string& name) const
{
    try
    {
        return m_Keybindings.at(group).at(name);
    }
    catch (const std::out_of_range&)
    {
        return "";
    }
}

// Clears the first (only) binding that has the same value as value
// Sets the group and name parameters to those of the binding that was cleared
// Returns true if it actually cleared a binding
// TODO: Add support for multiple keybindings per action
bool SettingsManager::clear_keybinding(const std::string& value,
                                       std::string& group,
                                       std::string& name)
{
    for (const auto& i : m_Keybindings)
    {
        for (const auto& j : i.second)
        {
            if (j.second == value)
            {
                group = i.first;
                name  = j.first;

                set_keybinding(group, name, "");

                return true;
            }
        }
    }

    return false;
}

void SettingsManager::set_keybinding(const std::string& group,
                                     const std::string& name,
                                     const std::string& value)
{
    if (!m_Config.exists("Keybindings"))
        m_Config.getRoot().add("Keybindings", Setting::TypeGroup);

    Setting& keys{ m_Config.lookup("Keybindings") };

    if (!keys.exists(group))
        keys.add(group, Setting::TypeGroup);

    set(name, value, Setting::TypeString, keys[group.c_str()]);
    m_Keybindings[group][name] = value;
}

std::string SettingsManager::reset_keybinding(const std::string& group, const std::string& name)
{
    if (m_Config.exists("Keybindings"))
    {
        Setting& keys{ m_Config.lookup("Keybindings") };

        if (keys.exists(group) && keys[group.c_str()].exists(name))
            keys[group.c_str()].remove(name);
    }

    try
    {
        return m_DefaultKeybindings.at(group).at(name);
    }
    catch (const std::out_of_range&)
    {
        return "";
    }
}

Gdk::RGBA SettingsManager::get_background_color() const
{
    if (m_Config.exists("BackgroundColor"))
        return Gdk::RGBA(static_cast<const char*>(m_Config.lookup("BackgroundColor")));

    return ImageBox::DefaultBGColor;
}

void SettingsManager::set_background_color(const Gdk::RGBA& value)
{
    set("BackgroundColor", value.to_string());
}

Rating SettingsManager::get_booru_max_rating() const
{
    if (m_Config.exists("BooruMaxRating"))
        return Rating(static_cast<int>(m_Config.lookup("BooruMaxRating")));

    return m_DefaultBooruMaxRating;
}

void SettingsManager::set_booru_max_rating(const Rating value)
{
    set("BooruMaxRating", static_cast<int>(value));
}

ZoomMode SettingsManager::get_zoom_mode() const
{
    if (m_Config.exists("ZoomMode"))
        return ZoomMode(static_cast<const char*>(m_Config.lookup("ZoomMode"))[0]);

    return m_DefaultZoomMode;
}

void SettingsManager::set_zoom_mode(const ZoomMode value)
{
    set("ZoomMode", std::string(1, static_cast<char>(value)));
}

Booru::TagViewOrder SettingsManager::get_tag_view_order() const
{
    if (m_Config.exists("TagViewOrder"))
        return Booru::TagViewOrder(static_cast<int>(m_Config.lookup("TagViewOrder")));

    return m_DefaultTagViewOrder;
}

void SettingsManager::set_tag_view_order(const Booru::TagViewOrder value)
{
    set("TagViewOrder", static_cast<int>(value));
}

void SettingsManager::remove(const std::string& key)
{
    if (m_Config.exists(key))
        m_Config.getRoot().remove(key);
}

void SettingsManager::load_keybindings()
{
    if (m_Config.exists("Keybindings"))
    {
        Setting& keys{ m_Config.lookup("Keybindings") };

        for (auto& i : m_DefaultKeybindings)
        {
            if (keys.exists(i.first))
            {
#ifdef HAVE_LIBPEAS
                // Assign keybindings only for plugins that have been loaded
                if (i.first == "Plugins")
                {
                    const auto& plugins{
                        Application::get_default()->get_plugin_manager().get_window_plugins()
                    };
                    for (auto& p : plugins)
                    {
                        if (!p->get_action_name().empty() &&
                            keys[i.first.c_str()].exists(p->get_action_name().c_str()))
                        {
                            m_Keybindings[i.first][p->get_action_name()] =
                                keys[i.first.c_str()][p->get_action_name().c_str()].c_str();
                        }
                    }
                    continue;
                }
#endif // HAVE_LIBPEAS
                for (auto& j : i.second)
                {
                    if (keys[i.first.c_str()].exists(j.first))
                    {
                        m_Keybindings[i.first][j.first] =
                            keys[i.first.c_str()][j.first.c_str()].c_str();
                    }
                    else
                    {
                        m_Keybindings[i.first][j.first] =
                            m_DefaultKeybindings.at(i.first).at(j.first);
                    }
                }
            }
            else
            {
                m_Keybindings[i.first] = m_DefaultKeybindings.at(i.first);
            }
        }
    }
    else
    {
        m_Keybindings = m_DefaultKeybindings;
    }
}

void SettingsManager::save_sites()
{
    remove("Sites");
    Setting& sites{ m_Config.getRoot().add("Sites", Setting::TypeList) };

    for (const std::shared_ptr<Site>& s : m_Sites)
    {
        Setting& site{ sites.add(Setting::TypeGroup) };

        set("name", s->get_name(), Setting::TypeString, site);
        set("url", s->get_url(), Setting::TypeString, site);
        set("type", static_cast<int>(s->get_type()), Setting::TypeInt, site);
        if (!s->get_username().empty())
            set("username", s->get_username(), Setting::TypeString, site);
#if !defined(HAVE_LIBSECRET) && !defined(_WIN32)
        if (!s->get_password().empty())
            set("password", s->get_password(), Setting::TypeString, site);
#endif // !defined(HAVE_LIBSECRET) && !defined(_WIN32)
        set("use_samples", s->use_samples(), Setting::TypeBoolean, site);
#ifdef HAVE_LIBPEAS
        if (s->get_type() == Type::PLUGIN)
            set("plugin_name", s->get_plugin_name(), Setting::TypeString, site);
#endif // HAVE_LIBPEAS
    }

    for (const auto& s : m_DisabledSites)
    {
        Setting& site{ sites.add(Setting::TypeGroup) };
        auto [name, url, username, password, type, use_samples, plugin_name] = s;

        set("name", name, Setting::TypeString, site);
        set("url", url, Setting::TypeString, site);
        set("type", static_cast<int>(type), Setting::TypeInt, site);

        if (!username.empty())
            set("username", username, Setting::TypeString, site);
#if !defined(HAVE_LIBSECRET) && !defined(_WIN32)
        if (!password.empty())
            set("password", password, Setting::TypeString, site);
#endif // !defined(HAVE_LIBSECRET) && !defined(_WIN32)
        set("use_samples", use_samples, Setting::TypeBoolean, site);
#ifdef HAVE_LIBPEAS
        if (type == Type::PLUGIN)
            set("plugin_name", plugin_name, Setting::TypeString, site);
#endif // HAVE_LIBPEAS
    }
}
