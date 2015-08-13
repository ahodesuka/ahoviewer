#ifndef _SITE_H_
#define _SITE_H_

#include <gtkmm.h>
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

            Site(std::string name, std::string url, Type type);
            ~Site();

            static std::shared_ptr<Site> create(const std::string &name, const std::string &url);
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

            std::string get_path();
            Glib::RefPtr<Gdk::Pixbuf> get_icon_pixbuf(const bool update = false);

            void save_tags() const;
            void set_row_values(Gtk::TreeRow row);

            Glib::Dispatcher& signal_icon_downloaded() { return m_SignalIconDownloaded; }
        private:
            static Type get_type_from_url(const std::string &url);

            static const std::map<Type, std::string> RequestURI,
                                                     PostURI;

            std::string m_Name, m_Url, m_IconPath, m_TagsPath, m_Path;
            Type m_Type;
            std::set<std::string> m_Tags;
            Curler m_Curler;

            Glib::RefPtr<Gdk::Pixbuf> m_IconPixbuf;
            Glib::Threads::Thread *m_IconCurlerThread;
            Glib::Dispatcher m_SignalIconDownloaded;

            sigc::connection m_IconDownloadedConn;
        };
    }
}

#endif /* _SITE_H_ */
