#include "application.h"
using namespace AhoViewer;

#include "booru/site.h"
#include "config.h"
#include "imagelist.h"
#include "mainwindow.h"
#include "settings.h"

#ifdef HAVE_LIBPEAS
#include "plugin/manager.h"
#endif // HAVE_LIBPEAS

#include <curl/curl.h>
// This can be removed once it's implemented in C++20
#include <date/tz.h>
#include <gtkmm.h>
#include <iostream>
#include <libxml/parser.h>

#ifdef USE_OPENSSL
#include <openssl/opensslv.h>
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
#include <deque>
#include <openssl/crypto.h>
#include <shared_mutex>

std::deque<std::shared_mutex> locks;

static void lock_callback(int mode, int type, const char*, int)
{
    if ((mode & CRYPTO_LOCK))
    {
        if ((mode & CRYPTO_READ))
            locks[type].lock_shared();
        else
            locks[type].lock();
    }
    else
    {
        if ((mode & CRYPTO_READ))
            locks[type].unlock_shared();
        else
            locks[type].unlock();
    }
}

static unsigned long thread_id()
{
    return static_cast<unsigned long>(pthread_self());
}

static void init_openssl_locks()
{
    locks.resize(CRYPTO_num_locks());
    CRYPTO_set_id_callback(&thread_id);
    CRYPTO_set_locking_callback(&lock_callback);
}
#endif // OPENSSL_VERSION_NUMBER < 0x10100000L
#endif // USE_OPENSSL

#ifdef USE_GNUTLS
#include <gcrypt.h>
#if (GCRYPT_VERSION_NUMBER < 0x010600)
GCRY_THREAD_OPTION_PTHREAD_IMPL;

void init_gnutls_locks()
{
    gcry_control(GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread);
}
#endif // GCRYPT_VERSION_NUMBER < 0x010600
#endif // USE_GNUTLS

#ifdef HAVE_GSTREAMER
#include <gst/gst.h>
#endif // HAVE_GSTREAMER

Application::Application()
    : Gtk::Application("com.github.ahodesuka.ahoviewer", Gio::APPLICATION_HANDLES_OPEN)
#ifdef HAVE_LIBPEAS
      ,
      m_PluginManager
{
    std::make_unique<Plugin::Manager>()
}
#endif // HAVE_LIBPEAS
{
    // Disgusting win32 api to start dbus-daemon and make it close when
    // the ahoviewer process ends
#ifdef _WIN32
    HANDLE job                                = CreateJobObject(NULL, NULL);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
    PROCESS_INFORMATION pi                    = { 0 };
    STARTUPINFO si                            = { 0 };
    char cmd[]                                = "dbus-daemon.exe --session";

    jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(job, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
    si.cb = sizeof(si);

    if (CreateProcess(NULL,
                      cmd,
                      NULL,
                      NULL,
                      FALSE,
                      CREATE_NO_WINDOW | CREATE_SUSPENDED | CREATE_BREAKAWAY_FROM_JOB,
                      NULL,
                      NULL,
                      &si,
                      &pi))
    {
        AssignProcessToJobObject(job, pi.hProcess);
        ResumeThread(pi.hThread);
    }
#endif // _WIN32
    // The ImageBox needs this, disabling it is silly unless proven otherwise
    Glib::setenv("GTK_OVERLAY_SCROLLING", "", true);
    Glib::set_application_name(PACKAGE);

    signal_shutdown().connect(sigc::mem_fun(*this, &Application::on_shutdown));
}

Application& Application::get_instance()
{
    static Application app;
    return app;
}

MainWindow* Application::create_window()
{
    Glib::RefPtr<Gtk::Builder> builder{ Gtk::Builder::create() };

    try
    {
        builder->add_from_resource("/ui/ahoviewer.ui");
    }
    catch (const Glib::Error& ex)
    {
        std::cerr << "Gtk::Builder::add_from_resource: " << ex.what() << std::endl;
        return nullptr;
    }

    MainWindow* w{ nullptr };
    builder->get_widget_derived("MainWindow", w);

    if (!w)
        throw std::runtime_error("Failed to create window");

    auto window{ static_cast<Gtk::Window*>(w) };
    add_window(*window);

    return w;
}

int Application::run(int argc, char** argv)
{
    register_application();

    if (!is_remote())
    {
        gtk_init(&argc, &argv);
#ifdef HAVE_GSTREAMER
        gst_init(&argc, &argv);
#endif // HAVE_GSTREAMER
    }

    return Gtk::Application::run(argc, argv);
}

void Application::on_activate()
{
    auto w{ create_window() };

    if (get_windows().size() == 1)
        w->restore_last_file();

    Gtk::Application::on_activate();
}

// Finds the first window with no local image list or creates a
// new window and then opens the file
void Application::on_open(const std::vector<Glib::RefPtr<Gio::File>>& f, const Glib::ustring&)
{
    auto windows{ get_windows() };
    auto it{ std::find_if(windows.cbegin(), windows.cend(), [](Gtk::Window* w) {
        return static_cast<MainWindow*>(w)->m_LocalImageList->empty();
    }) };

    if (it == windows.cend())
    {
        auto w{ create_window() };
        w->open_file(f.front()->get_path());
    }
    else
    {
        static_cast<MainWindow*>(*it)->open_file(f.front()->get_path());
    }
}

void Application::on_startup()
{
    LIBXML_TEST_VERSION

    curl_global_init(CURL_GLOBAL_DEFAULT);

    curl_version_info_data* ver_info{ curl_version_info(CURLVERSION_NOW) };
    std::string ssl_lib{ ver_info->ssl_version };

#if defined(USE_OPENSSL) && (OPENSSL_VERSION_NUMBER < 0x10100000L)
    if (ssl_lib.find("OpenSSL") != std::string::npos)
        init_openssl_locks();
#endif // defined(USE_OPENSSL) && (OPENSSL_VERSION_NUMBER < 0x10100000L)

#if defined(USE_GNUTLS) && GCRYPT_VERSION_NUMBER < 0x010600
    if (ssl_lib.find("GnuTLS") != std::string::npos)
        init_gnutls_locks();
#endif // defined(USE_GNUTLS) && GCRYPT_VERSION_NUMBER < 0x010600

    Gtk::Main::init_gtkmm_internals();

    Gtk::Application::on_startup();

    date::set_install(Glib::build_filename(Glib::get_user_cache_dir(), PACKAGE, "tzdata"));
    std::thread{ []() {
        try
        {
            date::get_tzdb();
        }
        catch (const std::runtime_error& e)
        {
            std::cerr << "Failed to fetch IANA timezone database, times will be displayed in UTC"
                      << std::endl;
        }
    } }.detach();

    // Load this after plugins have been loaded so the active plugins can have their keybindings set
    Settings.load_keybindings();
}

void Application::on_window_added(Gtk::Window* w)
{
    if (get_windows().size() == 0)
        static_cast<MainWindow*>(w)->m_OriginalWindow = true;

    Gtk::Application::on_window_added(w);
}

void Application::on_window_removed(Gtk::Window* w)
{
    Gtk::Application::on_window_removed(w);
    delete w;

    auto windows{ get_windows() };

    // Set a new original window if the previous was closed
    if (windows.size() > 0)
    {
        auto it{ std::find_if(windows.cbegin(), windows.cend(), [](Gtk::Window* mw) {
            return static_cast<MainWindow*>(mw)->m_OriginalWindow;
        }) };

        if (it == windows.cend())
            static_cast<MainWindow*>(windows.front())->m_OriginalWindow = true;
    }
}

void Application::on_shutdown()
{
    for (const std::shared_ptr<Booru::Site>& site : Settings.get_sites())
        site->save_tags();
}
