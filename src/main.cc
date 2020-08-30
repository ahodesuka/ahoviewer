#include "application.h"

int main(int argc, char** argv)
{
    return AhoViewer::Application::get_instance().run(argc, argv);
}
