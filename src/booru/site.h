#ifndef _SITE_H_
#define _SITE_H_

#include <gdkmm.h>
#include <set>

#include "curler.h"

namespace AhoViewer
{
    namespace Booru
    {
        class Site
        {
        public:
            enum class Rating
            {
                SAFE = 0,
                QUESTIONABLE,
                EXPLICIT,
            };

            enum class Type
            {
                DANBOORU = 0,
                GELBOORU,
                MOEBOORU,
                UNKNOWN,
            };

            ~Site();

            static std::shared_ptr<Site> create(const std::string &name,
                                                const std::string &url,
                                                const Type type = Type::UNKNOWN,
                                                const std::string &user = "",
                                                const std::string &pass = "");
            static const Glib::RefPtr<Gdk::Pixbuf>& get_missing_pixbuf();

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
            void set_password(const std::string &s) { m_NewAccount = true; m_Password = s; }

            std::string get_cookie();
            void cleanup_cookie() const;

            std::string get_path();
            Glib::RefPtr<Gdk::Pixbuf> get_icon_pixbuf(const bool update = false);

            void save_tags() const;

            Glib::Dispatcher& signal_icon_downloaded() { return m_SignalIconDownloaded; }
        protected:
            Site(const std::string &name,
                 const std::string &url,
                 const Type type,
                 const std::string &user,
                 const std::string &pass);
        private:
            static Type get_type_from_url(const std::string &url);

            static const std::map<Type, std::string> RequestURI,
                                                     PostURI,
                                                     RegisterURI;

            std::string m_Name,
                        m_Url,
                        m_Username,
                        m_Password,
                        m_IconPath,
                        m_TagsPath,
                        m_CookiePath,
                        m_Path;
            Type m_Type;
            bool m_NewAccount;
            uint64_t m_CookieTS;
            std::set<std::string> m_Tags;
            Curler m_Curler;

            Glib::RefPtr<Gdk::Pixbuf> m_IconPixbuf;
            Glib::Threads::Thread *m_IconCurlerThread;
            Glib::Dispatcher m_SignalIconDownloaded;
        };
    }
}

#endif /* _SITE_H_ */
