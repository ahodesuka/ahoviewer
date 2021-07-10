#pragma once

#include "config.h"

#include <gtkmm/aboutdialog.h>
#include <gtkmm/application.h>

#ifdef HAVE_LIBPEAS
#include "plugin/manager.h"
#endif // HAVE_LIBPEAS

#include "recentmenu.h"

namespace AhoViewer
{
    class MainWindow;
    class PreferencesDialog;
    class Application : public Gtk::Application
    {
    public:
        static Glib::RefPtr<Application> create();
        static Glib::RefPtr<Application> get_default();

        MainWindow* create_window(bool from_dnd = false);

        void set_accels_for_action(Glib::ustring action, const std::vector<Glib::ustring>& accels);
#ifdef HAVE_LIBPEAS
        Plugin::Manager& get_plugin_manager() const { return *m_PluginManager; }
#endif // HAVE_LIBPEAS
        PreferencesDialog* get_preferences_dialog() const { return m_PreferencesDialog; }
        Glib::RefPtr<RecentMenu> get_recent_menu() const { return m_RecentMenu; }

    protected:
        Application();

        void on_activate() override;
        void on_open(const std::vector<Glib::RefPtr<Gio::File>>& f, const Glib::ustring&) override;
        void on_startup() override;

        void on_report_issue();
        void on_quit();

        void on_window_added(Gtk::Window* w) override;
        void on_window_removed(Gtk::Window* w) override;

    private:
        PreferencesDialog* m_PreferencesDialog;
        Gtk::AboutDialog* m_AboutDialog;
        Glib::RefPtr<RecentMenu> m_RecentMenu;

        // Helper method that sets the dialog's transient for property-for property
        // to the most recently used window and shows it
        void show_dialog(Gtk::Window* dialog);
        void on_shutdown();

#ifdef HAVE_LIBPEAS
        std::unique_ptr<Plugin::Manager> m_PluginManager;
#endif // HAVE_LIBPEAS
    };
}
