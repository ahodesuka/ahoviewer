#include "siteplugin.h"
using namespace AhoViewer::Plugin;
using AhoViewer::Note;
using AhoViewer::Booru::PostDataTuple;
using AhoViewer::Booru::PostInfo;
using AhoViewer::Booru::Tag;

#include "util.h"
using AhoViewer::Util::null_check_string;

SitePlugin::SitePlugin(PeasPluginInfo* pi, AhoviewerSiteActivatable* a)
    : m_Activatable{ a },
      // This value is used to uniquely identify a particular plugin.
      // Sites that use it will save this name to know which plugin to look for
      m_Name{ peas_plugin_info_get_module_name(pi) }
{
    // Value here is ignored, only the key existing matters
    if (peas_plugin_info_get_external_data(pi, "No-Multiplexing"))
        m_Multiplexing = false;
}

std::string SitePlugin::get_test_uri() const
{
    return null_check_string(ahoviewer_site_activatable_get_test_uri(m_Activatable));
}

std::string SitePlugin::get_posts_uri(const std::string& tags, size_t page, size_t limit) const
{
    return null_check_string(
        ahoviewer_site_activatable_get_posts_uri(m_Activatable, tags.c_str(), page, limit));
}

std::string SitePlugin::get_register_url(const std::string& url) const
{
    return null_check_string(
        ahoviewer_site_activatable_get_register_url(m_Activatable, url.c_str()));
}

std::string SitePlugin::get_icon_url(const std::string& url) const
{
    return null_check_string(ahoviewer_site_activatable_get_icon_url(m_Activatable, url.c_str()));
}

std::tuple<std::vector<PostDataTuple>, size_t, std::string>
SitePlugin::parse_post_data(const unsigned char* data,
                            const size_t size,
                            const char* url,
                            const bool samples)
{
    AhoviewerPosts* posts{ ahoviewer_site_activatable_parse_post_data(
        m_Activatable, data, size, url, samples) };
    std::vector<PostDataTuple> posts_vec;
    size_t posts_count{ 0 };
    std::string error;

    if (posts)
    {
        posts_count = posts->count;
        if (posts->error)
            error = posts->error;

        if (error.empty())
        {
            for (guint i = 0; i < posts->posts->len; ++i)
            {
                AhoviewerPost* pd{ static_cast<AhoviewerPost*>(posts->posts->pdata[i]) };

                GPtrArray* tags{ pd->tags };
                std::vector<Tag> tags_vec;

                for (guint j = 0; j < tags->len; ++j)
                {
                    AhoviewerTag* td{ static_cast<AhoviewerTag*>(tags->pdata[j]) };
                    tags_vec.emplace_back(td->tag, static_cast<Tag::Type>(td->type));
                }

                PostInfo post_info{ Booru::format_date_time(static_cast<date::sys_seconds>(
                                        std::chrono::duration<gint64>(pd->date))),
                                    null_check_string(pd->source),
                                    Booru::get_rating_string(null_check_string(pd->rating)),
                                    null_check_string(pd->score) };

                posts_vec.emplace_back(null_check_string(pd->image_url),
                                       null_check_string(pd->thumb_url),
                                       null_check_string(pd->post_url),
                                       null_check_string(pd->notes_url),
                                       tags_vec,
                                       post_info);
            }
        }

        ahoviewer_posts_unref(posts);
    }

    return { posts_vec, posts_count, error };
}

std::vector<Note> SitePlugin::parse_note_data(const unsigned char* data, const size_t size)
{
    GPtrArray* notes{ ahoviewer_site_activatable_parse_note_data(m_Activatable, data, size) };
    std::vector<Note> notes_vec;

    if (notes)
    {
        for (guint i = 0; i < notes->len; ++i)
        {
            AhoviewerNote* nd{ static_cast<AhoviewerNote*>(notes->pdata[i]) };

            // A note without a body isn't useful
            if (!nd->body)
                continue;

            notes_vec.emplace_back(nd->body, nd->w, nd->h, nd->x, nd->y);
        }

        g_ptr_array_unref(notes);
    }

    return notes_vec;
}
