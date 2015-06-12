#ifndef _BROWSER_H_
#define _BROWSER_H_

#include <gtkmm.h>

#include "page.h"
#include "site.h"
#include "statusbar.h"
#include "tagentry.h"
#include "tagview.h"

#include "settings.h"

namespace AhoViewer
{
    namespace Booru
    {
        class Browser : public Gtk::VPaned
        {
        public:
            typedef sigc::signal<void, Page*> SignalPageChangedType;

            Browser(BaseObjectType *cobj, const Glib::RefPtr<Gtk::Builder> &bldr);
            virtual ~Browser();

            Page* get_active_page() const
                { return static_cast<Page*>(m_Notebook->get_nth_page(m_Notebook->get_current_page())); }
            int get_selected_booru() const { return m_ComboBox->get_active_row_number(); }
            Gtk::Entry* get_tag_entry() const { return m_TagEntry; }
            int get_min_width() const { return m_MinWidth; }
            std::string get_last_save_path() const { return m_LastSavePath; }

            void set_statusbar(StatusBar *sb) { m_StatusBar = sb; }
            void update_combobox_model();

            SignalPageChangedType signal_page_changed() const { return m_SignalPageChanged; }

            // Action callbacks {{{
            void on_new_tab();
            void on_close_tab();
            void on_save_image();
            void on_save_images();
            // }}}
        protected:
            virtual void on_realize();
        private:
            struct ComboBoxModelColumns : public Gtk::TreeModelColumnRecord
            {
                ComboBoxModelColumns() { add(pixbuf_column); add(text_column); }
                Gtk::TreeModelColumn<Glib::RefPtr<Gdk::Pixbuf>> pixbuf_column;
                Gtk::TreeModelColumn<std::string> text_column;
            };

            void close_page(Page *page);

            bool on_entry_key_press_event(GdkEventKey *e);
            void on_page_removed(Gtk::Widget*, guint);
            void on_switch_page(void*, guint);
            void on_imagelist_changed(const std::shared_ptr<AhoViewer::Image> &image);

            std::shared_ptr<Site> get_active_site() const
                { return Settings.get_sites()[m_ComboBox->get_active_row_number()]; }

            StatusBar *m_StatusBar;

            Gtk::ToolButton *m_NewTabButton,
                            *m_SaveImagesButton;
            Gtk::ComboBox *m_ComboBox;
            Gtk::Notebook *m_Notebook;

            ComboBoxModelColumns m_ComboColumns;
            Glib::RefPtr<Gtk::ListStore> m_ComboModel;

            TagEntry *m_TagEntry;
            TagView *m_TagView;

            bool m_IgnorePageSwitch;
            int m_MinWidth;
            std::string m_LastSavePath;

            Glib::RefPtr<Gtk::UIManager> m_UIManager;
            Glib::RefPtr<Gtk::Action> m_SaveImageAction,
                                      m_SaveImagesAction;

            sigc::connection m_ComboChangedConn,
                             m_NoResultsConn,
                             m_ImageListConn,
                             m_ImageProgConn,
                             m_SaveProgConn;

            SignalPageChangedType m_SignalPageChanged;
        };
    }
}

#endif /* _BROWSER_H_ */
