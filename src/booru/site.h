#ifndef _SITE_H_
#define _SITE_H_

#include <gdkmm.h>
#include <mutex>
#include <set>
#include <thread>

#include "config.h"
#include "curler.h"
#include "util.h"

namespace AhoViewer
{
    namespace Booru
    {
        class Site
        {
        public:
            ~Site();

            static std::shared_ptr<Site> create(const std::string &name,
                                                const std::string &url,
                                                const Type type = Type::UNKNOWN,
                                                const std::string &user = "",
                                                const std::string &pass = "",
                                                const unsigned int max_cons = 0,
                                                const bool use_samples = false);
            static const Glib::RefPtr<Gdk::Pixbuf>& get_missing_pixbuf();

#ifdef HAVE_LIBSECRET
            static void on_password_lookup(GObject*, GAsyncResult *result, gpointer ptr);
            static void on_password_stored(GObject*, GAsyncResult *result, gpointer ptr);
#endif // HAVE_LIBSECRET

            std::string get_posts_url(const std::string &tags, size_t page);
            std::string get_post_url(const std::string &id);
            void add_tags(const std::set<std::string> &tags);

            std::string get_name() const { return m_Name; }
            void set_name(const std::string &name) { m_Name = name; }

            std::string get_url() const { return m_Url; }
            bool set_url(const std::string &s);

            Type get_type() const { return m_Type; }
            const std::set<std::string>& get_tags() const { return m_Tags; }

            std::string get_register_uri() const { return m_Url + RegisterURI.at(m_Type); }

            std::string get_username() const { return m_Username; }
            void set_username(const std::string &s) { m_NewAccount = true; m_Username = s; }

            std::string get_password() const { return m_Password; }
            void set_password(const std::string &s);

            std::string get_cookie();
            void cleanup_cookie() const;

            int get_max_connections() const { return m_MaxConnections; }
            CURLSH* get_share_handle() const { return m_ShareHandle; }

            bool use_samples() const { return m_UseSamples; }
            void set_use_samples(const bool s) { m_UseSamples = s; }

            Glib::RefPtr<Gdk::Pixbuf> get_icon_pixbuf(const bool update = false);

            void save_tags() const;

            Glib::Dispatcher& signal_icon_downloaded() { return m_SignalIconDownloaded; }
#ifdef HAVE_LIBSECRET
            sigc::signal<void> signal_password_lookup() { return m_PasswordLookup; }
#endif // HAVE_LIBSECRET
        protected:
            Site(const std::string &name,
                 const std::string &url,
                 const Type type,
                 const std::string &user,
                 const std::string &pass,
                 const int max_cons,
                 const bool use_samples);
        private:
            static Type get_type_from_url(const std::string &url);

            static void share_lock_cb(CURL *c, curl_lock_data data, curl_lock_access access, void *userp);
            static void share_unlock_cb(CURL *c, curl_lock_data data, void *userp);

            static const std::map<Type, std::string> RequestURI,
                                                     PostURI,
                                                     RegisterURI;

            std::string m_Name,
                        m_Url,
                        m_Username,
                        m_Password,
                        m_IconPath,
                        m_TagsPath,
                        m_CookiePath;
            Type m_Type;
            bool m_NewAccount,
                 m_UseSamples;
            uint64_t m_CookieTS;
            std::set<std::string> m_Tags;
            int m_MaxConnections;
            CURLSH *m_ShareHandle;
            std::map<curl_lock_data, std::mutex> m_MutexMap;
            Curler m_Curler;

            Glib::RefPtr<Gdk::Pixbuf> m_IconPixbuf;
            std::thread m_IconCurlerThread;
            Glib::Dispatcher m_SignalIconDownloaded;

#ifdef HAVE_LIBSECRET
            sigc::signal<void> m_PasswordLookup;
#endif // HAVE_LIBSECRET
        };
    }
}

#endif /* _SITE_H_ */
