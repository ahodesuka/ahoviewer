#ifndef _APPLICATION_H_
#define _APPLICATION_H_

#include <gtkmm/application.h>

namespace AhoViewer
{
    class MainWindow;
    class Application : public Gtk::Application
    {
    public:
        static Glib::RefPtr<Application> create();

        MainWindow* create_window();

        int run(int argc, char** argv);

    protected:
        Application();

        void on_activate() override;
        void on_open(const std::vector<Glib::RefPtr<Gio::File>>& f, const Glib::ustring&) override;
        void on_startup() override;

        void on_window_added(Gtk::Window* w) override;
        void on_window_removed(Gtk::Window* w) override;

    private:
        void on_shutdown();
    };
}

#endif /* _APPLICATION_H_ */
