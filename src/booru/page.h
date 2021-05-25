#pragma once

#include "curler.h"
#include "imagelist.h"
#include "site.h"

#include <atomic>
#include <gtkmm.h>

namespace AhoViewer::Booru
{
    class Image;
    class Page : public Gtk::ScrolledWindow, public ImageList::Widget
    {
        friend class Browser;

        using SignalClosedType        = sigc::signal<void, Page*>;
        using SignalDownloadErrorType = sigc::signal<void, const std::string>;
        using SignalSaveProgressType  = sigc::signal<void, const Page*>;

    public:
        Page();
        ~Page() override;

        std::shared_ptr<Site> get_site() const { return m_Site; }
        std::shared_ptr<ImageList> get_imagelist() const { return m_ImageList; }
        size_t get_page_num() const { return m_Page; }
        Gtk::Label* get_menu_label() const { return m_MenuLabel; }

        SignalClosedType signal_closed() const { return m_SignalClosed; }
        SignalDownloadErrorType signal_no_results() const { return m_SignalDownloadError; }
        SignalSaveProgressType signal_save_progress() const { return m_SignalSaveProgress; }
        sigc::signal<void> signal_posts_download_started() const
        {
            return m_SignalPostsDownloadStarted;
        }
        sigc::signal<void> signal_on_last_page() const { return m_SignalOnLastPage; }

    protected:
        void set_pixbuf(const size_t index, const Glib::RefPtr<Gdk::Pixbuf>& pixbuf) override;
        void set_selected(const size_t index) override;
        void scroll_to_selected() override;

    private:
        class IconView : public Gtk::IconView
        {
        public:
            IconView() : Gtk::IconView{} { }

        protected:
            // The default handler always stops event propagation and is only used for preliting and
            // rubberbanding
            bool on_motion_notify_event(GdkEventMotion* e) override { return false; }
        };
        // Custom CellRenderer that expands to center the pixbufs in the iconview
        class CellRendererThumbnail : public Gtk::CellRenderer /*{{{*/
        {
        public:
            CellRendererThumbnail()
                : Glib::ObjectBase{ typeid(CellRendererThumbnail) },
                  Gtk::CellRenderer{},
                  m_PixbufProperty{ *this, "pixbuf" },
                  m_PixbufRenderer{ Gtk::make_managed<Gtk::CellRendererPixbuf>() }
            {
                set_alignment(0.5, 0.5);
                m_PixbufProperty.get_proxy().signal_changed().connect(
                    [&]() { m_PixbufRenderer->property_pixbuf() = m_PixbufProperty.get_value(); });
            }
            ~CellRendererThumbnail() override = default;

            Glib::PropertyProxy<Glib::RefPtr<Gdk::Pixbuf>> property_pixbuf()
            {
                return m_PixbufProperty.get_proxy();
            }

            void get_preferred_width_vfunc(Gtk::Widget& widget,
                                           int& minimum_width,
                                           int& natural_width) const override;
            void get_preferred_height_vfunc(Gtk::Widget& widget,
                                            int& minimum_height,
                                            int& natural_height) const override
            {
                m_PixbufRenderer->get_preferred_height(widget, minimum_height, natural_height);
            }
            void render_vfunc(const Cairo::RefPtr<::Cairo::Context>& cr,
                              Gtk::Widget& widget,
                              const Gdk::Rectangle& background_area,
                              const Gdk::Rectangle& cell_area,
                              Gtk::CellRendererState flags) override
            {
                m_PixbufRenderer->render(cr, widget, background_area, cell_area, flags);
            }

            Glib::Property<Glib::RefPtr<Gdk::Pixbuf>> m_PixbufProperty;
            Gtk::CellRendererPixbuf* m_PixbufRenderer;
        }; /*}}}*/
        void search(const std::shared_ptr<Site>& site);
        void save_image(const std::string& path, const std::shared_ptr<Image>& img);
        void save_images(const std::string& path);
        bool ask_cancel_save();

        void cancel_save();
        void get_posts();
        void get_posts_tags();
        bool get_next_page();

        Gtk::Widget* get_tab() const { return m_Tab; }
        bool is_saving() const { return m_Saving; }

        void on_posts_downloaded();
        void on_selection_changed();
        void on_value_changed();
        bool on_button_press_event(GdkEventButton* e) override;
        bool on_tab_button_release_event(GdkEventButton* e);

        Gtk::Menu* m_PopupMenu;
        Gtk::IconView* m_IconView;
        Gtk::EventBox* m_Tab;
        Gtk::Image* m_TabIcon;
        Gtk::Label *m_TabLabel, *m_MenuLabel;
        Gtk::Button* m_TabButton;

        std::shared_ptr<ImageList> m_ImageList;
        std::shared_ptr<Site> m_Site;
        Curler m_Curler;

        // m_Tags stores tags that are inside the entry
        // while m_SearchTags are the whitespace trimmed tags used for the actual API query
        std::string m_Tags, m_SearchTags, m_Path, m_PostsError;
        double m_TagViewPos{ 0.0 };
        size_t m_Page{ 0 }, m_SaveImagesTotal{ 0 }, m_PostsCount{ 0 };
        std::atomic<size_t> m_SaveImagesCurrent{ 0 };
        std::atomic<bool> m_Saving{ false };
        bool m_LastPage{ false }, m_KeepAligned{ false };
        std::vector<PostDataTuple> m_Posts;

        Glib::RefPtr<Gio::Cancellable> m_SaveCancel;
        std::thread m_GetPostsThread, m_SaveImagesThread;
        Glib::Dispatcher m_SignalPostsDownloaded, m_SignalSaveProgressDisp;

        sigc::connection m_GetNextPageConn, m_ScrollConn;

        SignalClosedType m_SignalClosed;
        SignalDownloadErrorType m_SignalDownloadError;
        SignalSaveProgressType m_SignalSaveProgress;
        sigc::signal<void> m_SignalPostsDownloadStarted, m_SignalOnLastPage;
    };
}
