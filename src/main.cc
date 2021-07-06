#include "application.h"

#ifdef HAVE_GSTREAMER
#include <gst/gst.h>
#endif // HAVE_GSTREAMER

int main(int argc, char** argv)
{
#if defined(_WIN32) && defined(HAVE_LIBPEAS)
    // Prevent the generation of .pyc files
    Glib::setenv("PYTHONDONTWRITEBYTECODE", "1", true);
#endif // defined(_WIN32) && defined(HAVE_LIBPEAS)

#ifdef HAVE_GSTREAMER
    gst_init(&argc, &argv);
#endif // HAVE_GSTREAMER

    auto app{ AhoViewer::Application::create() };
    return app->run(argc, argv);
}
