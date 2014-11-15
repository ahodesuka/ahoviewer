#include <curl/curl.h>
#include <gtkmm.h>
#include <iostream>
#include <cstdlib>

#include "mainwindow.h"

extern const char ahoviewer_ui[];

int main(int argc, char **argv)
{
    curl_global_init(CURL_GLOBAL_ALL);

    Gtk::Main main(argc, argv);
    Glib::RefPtr<Gtk::Builder> builder = Gtk::Builder::create();

    try
    {
        builder->add_from_string(ahoviewer_ui);
    }
    catch (Gtk::BuilderError &ex)
    {
        std::cerr << "Gtk::BuilderError: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    AhoViewer::MainWindow *window = nullptr;
    builder->get_widget_derived("MainWindow", window);

    if (!window)
        return EXIT_FAILURE;

    window->show_all();

    if (argc == 2)
        window->open_file(argv[1]);
    else
        window->restore_last_file();

    main.run(*window);
    curl_global_cleanup();

    return EXIT_SUCCESS;
}
