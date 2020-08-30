#pragma once

#include "siteplugin.h"
#include "windowplugin.h"

#include <libpeas/peas.h>
#include <memory>
#include <vector>

namespace AhoViewer::Plugin
{
    class Manager
    {
    public:
        Manager();
        ~Manager();

        const auto& get_window_plugins() const { return m_WindowPlugins; }
        const auto& get_site_plugins() const { return m_SitePlugins; }

    private:
        PeasEngine* m_Engine{ nullptr };
        PeasExtensionSet *m_WindowExtensionSet{ nullptr }, *m_SiteExtensionSet{ nullptr };

        std::vector<std::unique_ptr<WindowPlugin>> m_WindowPlugins;
        std::vector<std::shared_ptr<SitePlugin>> m_SitePlugins;
    };
}
