#ifndef _MAINWINDOW_H_
#define _MAINWINDOW_H_

#include <gtkmm.h>

#include "booru/browser.h"
#include "imagebox.h"
#include "imagelist.h"
#include "preferences.h"
#include "statusbar.h"
#include "thumbnailbar.h"

namespace AhoViewer
{
    class MainWindow : public Gtk::Window
    {
    public:
        MainWindow(BaseObjectType *cobj, const Glib::RefPtr<Gtk::Builder> &bldr);
        virtual ~MainWindow() override = default;

        void open_file(const std::string &path, const int index = 0, const bool restore = false);
        void restore_last_file();
        void get_drawable_area_size(int &w, int &h) const;
    protected:
        virtual void on_realize() override;
        virtual void on_check_resize() override;
        virtual bool on_delete_event(GdkEventAny *e) override;
        virtual void on_drag_data_received(const Glib::RefPtr<Gdk::DragContext> &ctx, int, int,
                                           const Gtk::SelectionData &data, guint, guint time) override;
        virtual bool on_key_press_event(GdkEventKey *e) override;
    private:
        void set_active_imagelist(const std::shared_ptr<ImageList> &imageList);
        void save_window_geometry();
        void create_actions();
        void update_widgets_visibility();
        void set_sensitives();
        void set_booru_sensitives();
        void update_title();

        bool is_fullscreen() const;

        void on_imagelist_changed(const std::shared_ptr<Image> &image);
        void on_imagelist_cleared();
        void on_cache_size_changed();
        void on_accel_edited(const std::string &accelPath, const std::string &actionName);

        void on_connect_proxy(const Glib::RefPtr<Gtk::Action> &action, Gtk::Widget *w);

        // Action callbacks {{{
        void on_open_file_dialog();
        void on_open_recent_file();
        void on_close();
        void on_quit();
        void on_toggle_fullscreen();
        void on_toggle_manga_mode();
        void on_toggle_menu_bar();
        void on_toggle_status_bar();
        void on_toggle_booru_browser();
        void on_toggle_thumbnail_bar();
        void on_toggle_hide_all();
        void on_next_image();
        void on_previous_image();
        void on_last_image();
        void on_first_image();
        void on_toggle_slideshow();
        void on_save_image();
        // }}}

        Glib::RefPtr<Gtk::Builder> m_Builder;
        Glib::RefPtr<Gtk::ActionGroup> m_ActionGroup;
        Glib::RefPtr<Gtk::UIManager> m_UIManager;
        Gtk::MenuBar *m_MenuBar;
        Glib::RefPtr<Gtk::RecentAction> m_RecentAction;

        Gtk::AboutDialog *m_AboutDialog;
        PreferencesDialog *m_PreferencesDialog;
        ThumbnailBar *m_ThumbnailBar;
        ImageBox *m_ImageBox;
        StatusBar *m_StatusBar;
        Booru::Browser *m_BooruBrowser;
        Gtk::HPaned *m_HPaned;

        std::string m_LastSavePath;

        int m_Width, m_Height, m_HPanedMinPos, m_HPanedLastPos;
        // This keeps track of whether hide all was set automatically
        bool m_HideAllFullscreen;

        std::shared_ptr<ImageList> m_ActiveImageList,
                                   m_LocalImageList;
        sigc::connection m_ImageListConn,
                         m_ImageListClearedConn;
    };
}

#endif /* _MAINWINDOW_H_ */
