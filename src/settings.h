#pragma once

#include <gdkmm.h>
#include <glibmm.h>
#include <libconfig.h++>
using libconfig::Setting;

#include "util.h"

#include <map>
#include <vector>

namespace AhoViewer
{
    namespace Booru
    {
        class Site;
    }
    class SettingsManager
    {
        // name, url, type
        using SiteTuple = std::tuple<std::string, std::string, Booru::Type>;
        // name, url, username, pass, type, use_samples, plugin_name
        using DisabledSiteTuple = std::tuple<std::string,
                                             std::string,
                                             std::string,
                                             std::string,
                                             Booru::Type,
                                             bool,
                                             std::string>;
        using KeybindingMap     = std::vector<std::pair<std::string, std::vector<Glib::ustring>>>;

    public:
        SettingsManager();
        ~SettingsManager();

        bool get_bool(const std::string& key) const;
        int get_int(const std::string& key) const;
        std::string get_string(const std::string& key) const;

        std::vector<std::shared_ptr<Booru::Site>>& get_sites();
        void save_sites();

        // Returns a map of all keybindings
        const KeybindingMap& get_keybindings() const { return m_Keybindings; }
        // Returns the keybindings for a single action, if there are none it will
        // return an empty vector
        const std::vector<Glib::ustring> get_keybindings(const std::string& name) const;
        // Returns the default keybindings for a single action, if there are none it will
        // return an empty vector
        const std::vector<Glib::ustring> get_default_keybindings(const std::string& name) const;
        // Sets the given action's keybindings
        void set_keybindings(std::string name, const std::vector<Glib::ustring>& keys);
        // Clears the first (only) binding that has the same value as value
        // returns a pair with the action name and vector of new bindings
        // for the binding that was removed, or a pair with an empty string
        // and vector if no binding matched value
        std::pair<std::string, std::vector<Glib::ustring>>
        clear_keybinding(const std::string& value);
        // Resets the given action's keybindings back to it's default value
        void reset_keybindings(std::string name);
        void load_keybindings();

        void add_favorite_tag(const Booru::Tag& tag) { m_FavoriteTags.push_back(tag); }
        void remove_favorite_tag(const Booru::Tag& tag)
        {
            m_FavoriteTags.erase(std::remove(m_FavoriteTags.begin(), m_FavoriteTags.end(), tag),
                                 m_FavoriteTags.end());
        }
        std::vector<Booru::Tag>& get_favorite_tags() { return m_FavoriteTags; }

        bool get_geometry(int& x, int& y, int& w, int& h) const;
        void set_geometry(const int x, const int y, const int w, const int h);

        Gdk::RGBA get_background_color() const;
        void set_background_color(const Gdk::RGBA& value);

        Booru::Rating get_booru_max_rating() const;
        void set_booru_max_rating(const Booru::Rating value);

        ZoomMode get_zoom_mode() const;
        void set_zoom_mode(const ZoomMode value);

        Booru::TagViewOrder get_tag_view_order() const;
        void set_tag_view_order(const Booru::TagViewOrder value);

        const std::string get_booru_path() const { return m_BooruPath; }

        void remove(const std::string& key);

        void set(const std::string& key, const bool value)
        {
            set(key, value, Setting::TypeBoolean);
        }
        void set(const std::string& key, const int value) { set(key, value, Setting::TypeInt); }
        void set(const std::string& key, const std::string& value)
        {
            if (value.empty())
                remove(key);
            else
                set(key, value, Setting::TypeString);
        }

    private:
        libconfig::Config m_Config;

        const std::string m_ConfigPath, m_ConfigFilePath, m_BooruPath, m_FavoriteTagsPath;

        const std::map<std::string, bool> m_DefaultBools;
        const std::map<std::string, int> m_DefaultInts;
        const std::map<std::string, std::string> m_DefaultStrings;
        const std::vector<SiteTuple> m_DefaultSites;
        const KeybindingMap m_DefaultKeybindings;
        const Booru::Rating m_DefaultBooruMaxRating{ Booru::Rating::EXPLICIT };
        const ZoomMode m_DefaultZoomMode{ ZoomMode::MANUAL };
        const Booru::TagViewOrder m_DefaultTagViewOrder{ Booru::TagViewOrder::TYPE };

        std::vector<std::shared_ptr<Booru::Site>> m_Sites;
        std::vector<DisabledSiteTuple> m_DisabledSites;
        KeybindingMap m_Keybindings;
        std::vector<Booru::Tag> m_FavoriteTags;

        template<typename T>
        void set(const std::string& key, const T value, Setting::Type type)
        {
            set(key, value, type, m_Config.getRoot());
        }
        template<typename T>
        void set(const std::string& key, const T value, Setting::Type type, Setting& s)
        {
            if (!s.exists(key))
                s.add(key, type);

            s[key.c_str()] = value;
        }
    };

    extern SettingsManager Settings;
}
