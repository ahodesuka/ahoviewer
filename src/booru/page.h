#ifndef _PAGE_H_
#define _PAGE_H_

#include <gtkmm.h>

#include "site.h"
#include "imagefetcher.h"
#include "imagelist.h"

namespace AhoViewer
{
    namespace Booru
    {
        class Browser;
        class Page : public Gtk::ScrolledWindow,
                     public ImageList::Widget
        {
        public:
            typedef sigc::signal<void, Page*> SignalClosedType;
            typedef sigc::signal<void> SignalNoResultsType;

            Page(Browser *browser);
            virtual ~Page();

            virtual void set_selected(const size_t index);

            void search(std::shared_ptr<Site> site, const std::string &tags);

            ImageFetcher* get_image_fetcher() const;
            Gtk::Widget* get_tab() const { return m_Tab; }
            std::shared_ptr<Site> get_site() const { return m_Site; }
            std::shared_ptr<ImageList> get_imagelist() const { return m_ImageList; }
            std::string get_tags() const { return m_Tags; }

            SignalClosedType signal_closed() const { return m_SignalClosed; }
            SignalNoResultsType signal_no_results() const { return m_SignalNoResults; }
        private:
            void get_posts();
            void on_posts_downloaded();
            void on_selection_changed();
            void on_value_changed();

            Browser *m_Browser;
            Gtk::IconView *m_IconView;
            Gtk::HBox *m_Tab;
            Gtk::Image *m_TabIcon;
            Gtk::Label *m_TabLabel;
            Gtk::Button *m_TabButton;

            std::shared_ptr<ImageList> m_ImageList;
            std::shared_ptr<Site> m_Site;

            std::string m_Tags, m_Path;
            size_t m_Page, m_NumPosts;
            bool m_LastPage;
            pugi::xml_node m_Posts;

            Glib::Threads::Thread *m_GetPostsThread;
            Glib::Dispatcher m_SignalPostsDownloaded;

            SignalClosedType m_SignalClosed;
            SignalNoResultsType m_SignalNoResults;
        };
    }
}

#endif /* _PAGE_H_ */
