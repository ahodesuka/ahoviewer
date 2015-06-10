#ifndef _MAINWINDOW_H_
#define _MAINWINDOW_H_

#include <gtkmm.h>

#include "imagebox.h"
#include "imagelist.h"
#include "statusbar.h"
#include "thumbnailbar.h"
#include "booru/browser.h"

namespace AhoViewer
{
    class MainWindow : public Gtk::Window
    {
    public:
        MainWindow(BaseObjectType *cobj, const Glib::RefPtr<Gtk::Builder> &bldr);
        virtual ~MainWindow();

        void open_file(const std::string &path, const int index = 0);
        void restore_last_file();
    protected:
        virtual void on_realize();
        virtual void on_check_resize();
        virtual bool on_delete_event(GdkEventAny*);
        virtual void on_drag_data_received(const Glib::RefPtr<Gdk::DragContext> &ctx, int, int,
                                           const Gtk::SelectionData &data, guint, guint time);
        virtual bool on_key_press_event(GdkEventKey *e);
    private:
        void set_active_imagelist(std::shared_ptr<ImageList> imageList);
        void save_window_geometry();
        void create_actions();
        void hide_widgets();
        void show_widgets();
        void set_sensitives();
        void set_booru_sensitives();
        void update_title();
        void clear();

        bool is_fullscreen() const;

        void on_imagelist_changed(const std::shared_ptr<Image> &image);

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
        void placeholder();
        // }}}

        Glib::RefPtr<Gtk::Builder> m_Builder;
        Glib::RefPtr<Gtk::ActionGroup> m_ActionGroup;
        Glib::RefPtr<Gtk::UIManager> m_UIManager;
        Gtk::MenuBar *m_MenuBar;
        Gtk::RecentChooserMenu *m_RecentMenu;

        ThumbnailBar *m_ThumbnailBar;
        ImageBox *m_ImageBox;
        StatusBar *m_StatusBar;
        Booru::Browser *m_BooruBrowser;
        Gtk::HPaned *m_HPaned;

        std::shared_ptr<ImageList> m_ActiveImageList,
                                   m_LocalImageList;
        sigc::connection m_ImageListConn,
                         m_ImageListClearedConn;

        int m_Width, m_Height, m_HPanedMinPos, m_HPanedLastPos;
    };
}

#endif /* _MAINWINDOW_H_ */
