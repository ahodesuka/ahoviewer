#pragma once

#include "config.h"
#include "curler.h"
#ifdef HAVE_LIBPEAS
#include "plugin/siteplugin.h"
#endif // HAVE_LIBPEAS
#include "util.h"
#include "xml.h"

#include <gdkmm.h>
#include <mutex>
#include <set>
#include <thread>
#include <tuple>
#include <unordered_map>

namespace AhoViewer::Booru
{
    class Site
    {
    public:
        ~Site();

        // Default parameters here are used when creating default sites or when a new site is trying
        // to be created from the site editor
        static std::shared_ptr<Site> create(const std::string& name,
                                            const std::string& url,
                                            Type type              = Type::UNKNOWN,
                                            const std::string&     = "",
                                            const std::string&     = "",
                                            const bool use_samples = false);
        static const Glib::RefPtr<Gdk::Pixbuf>& get_missing_pixbuf();

#ifdef HAVE_LIBSECRET
        static void on_password_lookup(GObject*, GAsyncResult* result, gpointer ptr);
        static void on_password_stored(GObject*, GAsyncResult* result, gpointer ptr);
#endif // HAVE_LIBSECRET

        std::string get_posts_url(const std::string& tags, size_t page);
        void add_tags(const std::vector<Tag>& tags);

        std::string get_name() const { return m_Name; }
        void set_name(std::string name) { m_Name = std::move(name); }

#ifdef HAVE_LIBPEAS
        std::string get_plugin_name() const { return m_Plugin ? m_Plugin->get_name() : ""; }
        void set_plugin(std::shared_ptr<Plugin::SitePlugin> p) { m_Plugin = std::move(p); }
#endif // HAVE_LIBPEAS

        // At the moment this can only be controlled by plugins (no built in site type seems to have
        // a problem with multiplexing)
        bool get_multiplexing() const;

        std::string get_url() const { return m_Url; }
        bool set_url(std::string s);

        Type get_type() const { return m_Type; }
        const std::set<Tag>& get_tags() const { return m_Tags; }

        std::string get_register_url() const;

        std::string get_username() const { return m_Username; }
        void set_username(const std::string& s) { m_Username = s; }

        std::string get_password() const { return m_Password; }
        void set_password(const std::string& s);

        int get_max_connections() const { return m_MaxConnections; }
        CURLSH* get_share_handle() const { return m_ShareHandle; }

        bool use_samples() const { return m_UseSamples; }
        void set_use_samples(const bool s) { m_UseSamples = s; }

        Glib::RefPtr<Gdk::Pixbuf> get_icon_pixbuf(const bool update = false);

        void save_tags() const;

        std::tuple<std::vector<PostDataTuple>, size_t, std::string>
        parse_post_data(unsigned char* data, const size_t size);

        std::vector<Note> parse_note_data(unsigned char* data, const size_t size) const;

        Glib::Dispatcher& signal_icon_downloaded() { return m_SignalIconDownloaded; }
#ifdef HAVE_LIBSECRET
        sigc::signal<void> signal_password_lookup() { return m_PasswordLookup; }
#endif // HAVE_LIBSECRET
    protected:
        Site(std::string name,
             std::string url,
             const Type type,
             std::string user,
             std::string pass,
             const bool use_samples);

    private:
#ifdef HAVE_LIBPEAS
        static std::pair<Type, std::shared_ptr<Plugin::SitePlugin>>
#else  // !HAVE_LIBPEAS
        static Type
#endif // !HAVE_LIBPEAS
        get_type_from_url(const std::string& url);

        static void
        share_lock_cb(CURL* c, curl_lock_data data, curl_lock_access access, void* userp);
        static void share_unlock_cb(CURL* c, curl_lock_data data, void* userp);

        std::unordered_map<std::string, Tag::Type> get_posts_tags(const xml::Document& posts) const;

        static const std::map<Type, std::string> RequestURI, PostURI, NotesURI, RegisterURI;

        std::string m_Name, m_Url, m_Username, m_Password, m_IconPath, m_TagsPath, m_CookiePath;
        Type m_Type;
        bool m_UseSamples;
        uint64_t m_CookieTS{ 0 };
        std::set<Tag> m_Tags;

        std::unordered_map<std::string, Tag::Type> m_MoebooruTags;
        std::mutex m_TagMutex;

        int m_MaxConnections{ 0 };
        CURLSH* m_ShareHandle;
        std::map<curl_lock_data, std::mutex> m_MutexMap;
        Curler m_Curler;
#ifdef HAVE_LIBPEAS
        std::shared_ptr<Plugin::SitePlugin> m_Plugin{ nullptr };
#endif // HAVE_LIBPEAS

        Glib::RefPtr<Gdk::Pixbuf> m_IconPixbuf;
        std::thread m_IconCurlerThread;
        Glib::Dispatcher m_SignalIconDownloaded;

#ifdef HAVE_LIBSECRET
        sigc::signal<void> m_PasswordLookup;
#endif // HAVE_LIBSECRET
    };
}
