#pragma once

#include <gtkmm.h>

namespace AhoViewer
{
    class Application;
    class Image;
    class ImageBox;
    class ImageList;
    class StatusBar;
    class ThumbnailBar;
    namespace Booru
    {
        class Browser;
        class InfoBox;
        class TagEntry;
        class TagView;
    }
    class MainWindow : public Gtk::ApplicationWindow
    {
        friend Application;
        friend Booru::Browser;

    public:
        MainWindow();
        ~MainWindow() override;

        void show();
        void open_file(const Glib::ustring& uri, const int index = 0, const bool restore = false);
        void restore_last_file();
        void get_drawable_area_size(int& w, int& h) const;

    protected:
        bool on_key_press_event(GdkEventKey* e) override;
        void on_realize() override;
        void on_size_allocate(Gtk::Allocation& alloc) override;
        bool on_delete_event(GdkEventAny* e) override;
        bool on_window_state_event(GdkEventWindowState* e) override;
        void on_drag_data_received(const Glib::RefPtr<Gdk::DragContext>& ctx,
                                   int,
                                   int,
                                   const Gtk::SelectionData& data,
                                   guint,
                                   guint time) override;
        bool on_motion_notify_event(GdkEventMotion* e) override;

    private:
        void quit();
        void set_active_imagelist(const std::shared_ptr<ImageList>& image_list);
        void save_window_geometry();
        void add_actions();
        void update_widgets_visibility();
        void set_sensitives();
        void set_booru_sensitives();
        void update_title();
        void save_image_as();

        bool is_fullscreen() const;

        void on_imagelist_changed(const std::shared_ptr<Image>& image);
        void on_imagelist_cleared();
        void on_cache_size_changed();

        // Action callbacks {{{
        void on_open_file_dialog();
        void on_open_recent(const Glib::VariantBase& value);
        void on_close();
        void on_toggle_fullscreen(bool state);
        void on_toggle_manga_mode(bool state);
        void on_toggle_menu_bar(bool state);
        void on_toggle_status_bar(bool state);
        void on_toggle_scrollbars(bool state);
        void on_toggle_booru_browser(bool state);
        void on_toggle_thumbnail_bar(bool state);
        void on_toggle_hide_all(bool state);
        void on_next_image();
        void on_previous_image();
        void on_last_image();
        void on_first_image();
        void on_toggle_slideshow(bool state);
        void on_toggle_auto_hide_info_box(bool state);
        void on_save_image();
        void on_save_image_as();
        void on_delete_image();
        // }}}

        Gtk::MessageDialog* m_AskDeleteConfirmDialog;
        Gtk::CheckButton* m_AskDeleteConfirmCheckButton;

        ThumbnailBar* m_ThumbnailBar;
        ImageBox* m_ImageBox;
        StatusBar* m_StatusBar;
        Booru::InfoBox* m_InfoBox;
        Gtk::Paned* m_HPaned;
        Booru::Browser* m_BooruBrowser;
        Booru::TagView* m_TagView;
        Booru::TagEntry* m_TagEntry;

        std::string m_LastSavePath;

        int m_Width{ 0 }, m_Height{ 0 }, m_HPanedLastPos{ 0 }, m_LastXPos{ 0 }, m_LastYPos{ 0 };
        // This keeps track of whether hide all was set automatically to prevent
        // the setting entry from being set
        bool m_HideAllFullscreen{ false },
            // This is to prevent hideall from being toggled off after unfullscreening
            // if it was set manually before fullscreening
            m_WasHideAll{ false },
            // Tracks whether this was the only window at one point
            m_OriginalWindow{ false }, m_IsMinimized{ false };
        // This is used to attempt to keep track of which window was last used when quitting
        // if there are multiple windows open we use the most recent time_point to save the
        // windows geometry
        std::chrono::time_point<std::chrono::steady_clock> m_LastSizeAllocateTime;

        std::shared_ptr<ImageList> m_ActiveImageList, m_LocalImageList;
        sigc::connection m_ImageListConn, m_ImageListClearedConn;
    };
}
