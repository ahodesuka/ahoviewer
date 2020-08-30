#include "mainwindowinterface.h"

#include "mainwindow.h"
using AhoViewer::MainWindow;

extern "C"
{
    void mainwindow_open_file(CMainWindow* w, const char* path)
    {
        MainWindow* main_window{ reinterpret_cast<MainWindow*>(w) };
        main_window->open_file(path);
    }
}
