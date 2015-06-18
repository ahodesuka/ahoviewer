#include <curl/curl.h>
#include <gtkmm.h>
#include <iostream>
#include <cstdlib>

#include "config.h"
#include "mainwindow.h"

#ifdef HAVE_GSTREAMER
#include <gst/gst.h>
#endif // HAVE_GSTREAMER

extern const unsigned char ahoviewer_ui[];
extern const unsigned long ahoviewer_ui_size;

int main(int argc, char **argv)
{
    curl_global_init(CURL_GLOBAL_ALL);

    Gtk::Main main(argc, argv);
#ifdef HAVE_GSTREAMER
    gst_init(&argc, &argv);
#endif // HAVE_GSTREAMER
    Glib::RefPtr<Gtk::Builder> builder = Gtk::Builder::create();

    try
    {
        builder->add_from_string(reinterpret_cast<const char*>(ahoviewer_ui),
                                 ahoviewer_ui_size);
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

    if (argc == 2)
        window->open_file(argv[1]);
    else
        window->restore_last_file();

    main.run(*window);
    curl_global_cleanup();

    return EXIT_SUCCESS;
}
