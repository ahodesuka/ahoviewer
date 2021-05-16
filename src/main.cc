#include "application.h"

int main(int argc, char** argv)
{
#if defined(_WIN32) && defined(HAVE_LIBPEAS)
    // Prevent the generation of .pyc files
    Glib::setenv("PYTHONDONTWRITEBYTECODE", "1", true);
#endif // defined(_WIN32) && defined(HAVE_LIBPEAS)

    return AhoViewer::Application::get_instance().run(argc, argv);
}
