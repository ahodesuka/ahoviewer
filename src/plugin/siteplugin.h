#pragma once

#include "siteactivatable.h"
#include "util.h"

#include <libpeas/peas.h>
#include <string>

namespace AhoViewer::Plugin
{
    class SitePlugin
    {
    public:
        SitePlugin(PeasPluginInfo* pi, AhoviewerSiteActivatable* const a);
        ~SitePlugin() = default;

        std::string get_name() const { return m_Name; }
        bool get_multiplexing() const { return m_Multiplexing; }

        std::string get_test_uri() const;
        std::string get_posts_uri(const std::string& tags, size_t page, size_t limit) const;
        std::string get_register_url(const std::string& tags) const;
        std::string get_icon_url(const std::string& url) const;

        std::tuple<std::vector<Booru::PostDataTuple>, size_t, std::string>
        parse_post_data(const unsigned char* data,
                        const size_t size,
                        const char* url,
                        const bool samples);
        std::vector<Note> parse_note_data(const unsigned char* data, const size_t size);

    private:
        AhoviewerSiteActivatable* m_Activatable;

        std::string m_Name;
        bool m_Multiplexing{ true };
    };
}
