#pragma once

#include "page.h"
#include "settings.h"
#include "site.h"
#include "statusbar.h"
#include "tagentry.h"
#include "tagview.h"

#include <gtkmm.h>

namespace AhoViewer
{
    class MainWindow;
    namespace Booru
    {
        class InfoBox;
        class Browser : public Gtk::Paned
        {
            friend MainWindow;
            using SignalPageChangedType = sigc::signal<void, Page*>;

        public:
            Browser(BaseObjectType* cobj, const Glib::RefPtr<Gtk::Builder>& bldr);
            ~Browser() override;

            std::vector<Page*> get_pages() const;
            Page* get_active_page() const
            {
                return static_cast<Page*>(m_Notebook->get_nth_page(m_Notebook->get_current_page()));
            }
            int get_selected_booru() const { return m_ComboBox->get_active_row_number(); }
            Gtk::Entry* get_tag_entry() const { return m_TagEntry; }
            std::string get_last_save_path() const { return m_LastSavePath; }

            void update_combobox_model();

            SignalPageChangedType signal_page_changed() const { return m_SignalPageChanged; }
            sigc::signal<void> signal_entry_blur() const { return m_SignalEntryBlur; }

            // Action callbacks {{{
            void on_new_tab();
            void on_close_tab();
            void on_save_image();
            void on_save_image_as();
            void on_save_images();
            void on_view_post() const;
            void on_copy_image_url();
            void on_copy_image_data();
            void on_copy_post_url();
            // }}}
        protected:
            void on_realize() override;
            void on_show() override;

        private:
            struct ComboBoxModelColumns : public Gtk::TreeModelColumnRecord
            {
                ComboBoxModelColumns()
                {
                    add(icon);
                    add(name);
                }
                Gtk::TreeModelColumn<Glib::RefPtr<Gdk::Pixbuf>> icon;
                Gtk::TreeModelColumn<std::string> name;
            };

            void search(const bool new_tab);
            void close_page(Page* page);
            void on_save_progress(const Page* p);
            void on_image_progress(const Image* bimage, double c, double t);

            bool on_entry_key_press_event(GdkEventKey* e);
            void on_entry_button_release_event(const Gtk::EntryIconPosition&,
                                               const GdkEventButton* e);
            void on_entry_value_changed();
            void on_page_removed(Gtk::Widget* w, guint);
            void on_page_removed_cleanup();
            void on_page_added(Gtk::Widget* w, guint);
            void on_switch_page(Gtk::Widget* w, guint);
            void on_imagelist_changed(const std::shared_ptr<AhoViewer::Image>& image);
            void on_new_tab_tag(const std::string& tag);

            static GtkNotebook*
            on_create_window(GtkNotebook*, GtkWidget*, gint x, gint y, gpointer*);

            bool check_saving_page();
            void connect_image_signals(const std::shared_ptr<Image> bimage);
            void save_image_as();
            void reset_tag_entry_progress();

            std::shared_ptr<Site> get_active_site() const
            {
                return Settings.get_sites()[m_ComboBox->get_active_row_number()];
            }

            StatusBar* m_StatusBar;

            Gtk::Button *m_NewTabButton, *m_SaveImagesButton;
            Gtk::ComboBox* m_ComboBox;
            Gtk::Notebook* m_Notebook;
            Gtk::Menu* m_PopupMenu;

            ComboBoxModelColumns m_ComboColumns;
            Glib::RefPtr<Gtk::ListStore> m_ComboModel;

            TagEntry* m_TagEntry;
            InfoBox* m_InfoBox;
            TagView* m_TagView;
            Page* m_CurrentPage{ nullptr };

            bool m_ClosePage{ false },
                // This is used to prevent the tagview position from
                // shrinking when first being shown (when it isnt visible on
                // startup), not exactly sure what causes this but this
                // workaround works for now.
                m_FirstShow{ true };
            std::string m_LastSavePath;

            Glib::RefPtr<Gtk::UIManager> m_UIManager;
            Glib::RefPtr<Gtk::Action> m_SaveImageAction, m_SaveImagesAction;

            std::map<Page*, sigc::connection> m_PageCloseConns;
            std::vector<sigc::connection> m_SiteIconConns;
            sigc::connection m_ComboChangedConn, m_DownloadErrorConn, m_ImageListConn,
                m_ImageProgConn, m_ImageErrorConn, m_PosChangedConn, m_SaveProgConn,
                m_PageSwitchedConn, m_PostsDownloadStartedConn, m_PostsDownloadFinishedConn,
                m_PostsDownloadPulseConn, m_PostsLoadProgressConn, m_PostsOnLastPageConn;

            SignalPageChangedType m_SignalPageChanged;
            sigc::signal<void> m_SignalEntryBlur;
        };
    }
}
