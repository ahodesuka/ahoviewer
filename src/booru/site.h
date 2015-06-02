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
                SAFE,
                QUESTIONABLE,
                EXPLICIT,
            };

            enum class Type
            {
                DANBOORU,
                GELBOORU,
            };

            Site(std::string name, std::string url, Type type);
            ~Site();

            static Type string_to_type(std::string type);

            std::string get_url(const std::string &tags, size_t page);
            void add_tags(const std::set<std::string> &tags);

            std::string get_name() const { return m_Name; }
            std::string get_url() const { return m_Url; }
            Glib::RefPtr<Gdk::Pixbuf> get_icon_pixbuf() const { return m_IconPixbuf; }
            const std::set<std::string>& get_tags() const { return m_Tags; }
            std::string get_path();

            void save_tags() const;
            void set_row_values(Gtk::TreeModel::Row row);
        private:
            static const Glib::RefPtr<Gdk::Pixbuf>& get_missing_pixbuf();

            static const std::map<Type, std::string> RequestURI;

            std::string m_Name, m_Url, m_IconPath, m_TagsPath, m_Path;
            Type m_Type;
            std::set<std::string> m_Tags;
            Curler m_Curler;

            Glib::RefPtr<Gdk::Pixbuf> m_IconPixbuf;
            Glib::Threads::Thread *m_IconCurlerThread;
            Glib::Dispatcher m_SignalIconDownloaded;
        };
    }
}

#endif /* _SITE_H_ */
