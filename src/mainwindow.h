#pragma once

#include <gtkmm.h>

namespace AhoViewer
{
    class Application;
    class Image;
    class ImageBox;
    class ImageList;
    class PreferencesDialog;
    class StatusBar;
    class ThumbnailBar;
    namespace Booru
    {
        class Browser;
        class InfoBox;
        class TagView;
    }
    class MainWindow : public Gtk::ApplicationWindow
    {
        friend Application;
        friend Booru::Browser;

    public:
        MainWindow(BaseObjectType* cobj, Glib::RefPtr<Gtk::Builder> bldr);
        ~MainWindow() override;

        void open_file(const std::string& path, const int index = 0, const bool restore = false);
        void restore_last_file();
        void get_drawable_area_size(int& w, int& h) const;

        bool has_rgba_visual() const { return m_HasRGBAVisual; }

    protected:
        void on_realize() override;
        void on_check_resize() override;
        bool on_delete_event(GdkEventAny* e) override;
        bool on_window_state_event(GdkEventWindowState* e) override;
        void on_drag_data_received(const Glib::RefPtr<Gdk::DragContext>& ctx,
                                   int,
                                   int,
                                   const Gtk::SelectionData& data,
                                   guint,
                                   guint time) override;
        bool on_key_press_event(GdkEventKey* e) override;
        bool on_motion_notify_event(GdkEventMotion* e) override;

    private:
        void set_active_imagelist(const std::shared_ptr<ImageList>& image_list);
        void save_window_geometry();
        void create_actions();
        void update_widgets_visibility();
        void set_sensitives();
        void set_booru_sensitives();
        void update_title();
        void save_image_as();

        bool is_fullscreen() const;

        void on_imagelist_changed(const std::shared_ptr<Image>& image);
        void on_imagelist_cleared();
        void on_cache_size_changed();
        void on_accel_edited(const std::string& accel_path, const std::string& action_name);

        void on_connect_proxy(const Glib::RefPtr<Gtk::Action>& action, Gtk::Widget* w);

        static void on_screen_changed(GtkWidget* w, GdkScreen* prev, gpointer userp);

        // Action callbacks {{{
        void on_open_file_dialog();
        void on_show_preferences();
        void on_show_about();
        void on_open_recent_file();
        void on_close();
        void on_quit();
        void on_toggle_fullscreen();
        void on_toggle_manga_mode();
        void on_toggle_menu_bar();
        void on_toggle_status_bar();
        void on_toggle_scrollbars();
        void on_toggle_booru_browser();
        void on_toggle_thumbnail_bar();
        void on_toggle_hide_all();
        void on_next_image();
        void on_previous_image();
        void on_last_image();
        void on_first_image();
        void on_toggle_slideshow();
        void on_save_image();
        void on_save_image_as();
        void on_delete_image();
        // }}}

        static PreferencesDialog* m_PreferencesDialog;
        static Gtk::AboutDialog* m_AboutDialog;
        static Gtk::MessageDialog* m_AskDeleteConfirmDialog;
        static Gtk::CheckButton* m_AskDeleteConfirmCheckButton;

        Glib::RefPtr<Gtk::Builder> m_Builder;
        Glib::RefPtr<Gtk::ActionGroup> m_ActionGroup;
        Glib::RefPtr<Gtk::UIManager> m_UIManager;
        Gtk::MenuBar* m_MenuBar;
        Glib::RefPtr<Gtk::RecentAction> m_RecentAction;

        ThumbnailBar* m_ThumbnailBar;
        ImageBox* m_ImageBox;
        StatusBar* m_StatusBar;
        Booru::Browser* m_BooruBrowser;
        Booru::InfoBox* m_InfoBox;
        Gtk::Paned* m_HPaned;
        Booru::TagView* m_TagView;

        std::string m_LastSavePath;

        int m_Width{ 0 }, m_Height{ 0 }, m_HPanedLastPos{ 0 }, m_LastXPos{ 0 }, m_LastYPos{ 0 };
        // This keeps track of whether hide all was set automatically to prevent
        // the setting entry from being set
        bool m_HideAllFullscreen{ false },
            // This is to prevent hideall from being toggled off after unfullscreening
            // if it was set manually before fullscreening
            m_WasHideAll{ false },
            // Tracks whether this was the only window at one point
            m_OriginalWindow{ false }, m_IsMinimized{ false }, m_HasRGBAVisual{ false };

        std::shared_ptr<ImageList> m_ActiveImageList, m_LocalImageList;
        sigc::connection m_ImageListConn, m_ImageListClearedConn;
    };
}
