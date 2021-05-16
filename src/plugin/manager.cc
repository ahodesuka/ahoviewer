#include "manager.h"
using namespace AhoViewer::Plugin;

#include "config.h"
#include "siteactivatable.h"
#include "windowabstract.h"

#include <algorithm>
#include <glibmm.h>
#include <iostream>

Manager::Manager() : m_Engine{ peas_engine_get_default() }
{
    peas_engine_enable_loader(m_Engine, "python3");
    peas_engine_enable_loader(m_Engine, "lua5.1");

    auto user_plugin_dir{ Glib::build_filename(Glib::get_user_data_dir(), PACKAGE, "plugins") };
    peas_engine_prepend_search_path(m_Engine, user_plugin_dir.c_str(), nullptr);
#ifdef _WIN32
    // Check for plugins inside the plugins dir packaged with the exe
    gchar* g{ g_win32_get_package_installation_directory_of_module(NULL) };
    if (g)
    {
        std::string plugin_path{ Glib::build_filename(g, "plugins") };
        peas_engine_prepend_search_path(m_Engine, plugin_path.c_str(), nullptr);

        g_free(g);
    }
#else  // !_WIN32
    auto gir_dir{ Glib::build_filename(AHOVIEWER_PREFIX, "lib", "girepository-1.0") };
    g_irepository_prepend_search_path(gir_dir.c_str());
#endif // !_WIN32

    m_WindowExtensionSet =
        peas_extension_set_new(m_Engine, AHOVIEWER_WINDOW_TYPE_ABSTRACT, nullptr);
    m_SiteExtensionSet = peas_extension_set_new(m_Engine, AHOVIEWER_SITE_TYPE_ACTIVATABLE, nullptr);

    peas_engine_rescan_plugins(m_Engine);

    const GList* ps{ peas_engine_get_plugin_list(m_Engine) };

    for (; ps != nullptr; ps = ps->next)
    {
        auto p{ static_cast<PeasPluginInfo*>(ps->data) };

        if (peas_engine_load_plugin(m_Engine, p))
        {
            bool found{ false };

            // Plugins could potentially implement multiple activatable interfaces
            if (peas_engine_provides_extension(m_Engine, p, AHOVIEWER_SITE_TYPE_ACTIVATABLE))
            {
                auto aa{ AHOVIEWER_SITE_ACTIVATABLE(
                    peas_extension_set_get_extension(m_SiteExtensionSet, p)) };
                auto plugin{ std::make_shared<SitePlugin>(p, aa) };
                m_SitePlugins.push_back(plugin);
                found = true;
            }

            if (peas_engine_provides_extension(m_Engine, p, AHOVIEWER_WINDOW_TYPE_ABSTRACT))
            {
                auto aa{ AHOVIEWER_WINDOW_ABSTRACT(
                    peas_extension_set_get_extension(m_WindowExtensionSet, p)) };
                m_WindowPlugins.push_back(std::make_unique<WindowPlugin>(p, aa));
                found = true;
            }

            if (!found)
            {
                std::cerr << "Plugin '" << peas_plugin_info_get_name(p)
                          << "' does not implement a known extension" << std::endl;
            }
        }
        else
        {
            std::cerr << "Failed to load plugin " << peas_plugin_info_get_name(p) << std::endl;
        }
    }

    // Sort window plugins alphabetically by their names so they show up in a consistent order in
    // the menu
    std::sort(m_WindowPlugins.begin(), m_WindowPlugins.end(), [](const auto& a, const auto& b) {
        return a->get_name() < b->get_name();
    });
}

Manager::~Manager()
{
    m_WindowPlugins.clear();
    m_SitePlugins.clear();

    g_object_unref(m_SiteExtensionSet);
    g_object_unref(m_WindowExtensionSet);
    g_object_unref(m_Engine);
}
