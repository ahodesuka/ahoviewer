#pragma once

#include "config.h"

#include <gtkmm/application.h>

#ifdef HAVE_LIBPEAS
#include "plugin/manager.h"
#endif // HAVE_LIBPEAS

namespace AhoViewer
{
    class MainWindow;
    class Application : public Gtk::Application
    {
    public:
        static Glib::RefPtr<Application> create();
        static Glib::RefPtr<Application> get_default();

        MainWindow* create_window();

#ifdef HAVE_LIBPEAS
        Plugin::Manager& get_plugin_manager() const { return *m_PluginManager; }
#endif // HAVE_LIBPEAS
    protected:
        Application();

        void on_activate() override;
        void on_open(const std::vector<Glib::RefPtr<Gio::File>>& f, const Glib::ustring&) override;
        void on_startup() override;

        void on_window_added(Gtk::Window* w) override;
        void on_window_removed(Gtk::Window* w) override;

    private:
        void on_shutdown();

#ifdef HAVE_LIBPEAS
        std::unique_ptr<Plugin::Manager> m_PluginManager;
#endif // HAVE_LIBPEAS
    };
}
