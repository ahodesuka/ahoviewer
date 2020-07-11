#include <gtkmm.h>
#include <iostream>
#include <curl/curl.h>
#include <libxml/parser.h>

#include "application.h"
using namespace AhoViewer;

#include "config.h"
#include "mainwindow.h"

#ifdef USE_OPENSSL
#include <openssl/opensslv.h>
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
#include <openssl/crypto.h>
#include <shared_mutex>
#include <deque>

std::deque<std::shared_timed_mutex> locks;

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

static void glibmm_log_filter(const gchar *ld, GLogLevelFlags ll, const gchar *msg, gpointer ud)
{
    if (strcmp(msg, "Dropped dispatcher message as the dispatcher no longer exists") != 0)
        g_log_default_handler(ld, ll, msg, ud);
}

Application::Application()
  : Gio::Application("com.github.ahodesuka.ahoviewer", Gio::APPLICATION_HANDLES_OPEN)
{
    // Disgusting win32 api to start dbus-daemon and make it close when
    // the ahoviewer process ends
#ifdef _WIN32
    HANDLE job = CreateJobObject(NULL, NULL);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
    PROCESS_INFORMATION pi = { 0 };
    STARTUPINFO si = { 0 };
    char cmd[] = "dbus-daemon.exe --session";

    jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(job, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
    si.cb = sizeof(si);

    if (CreateProcess(NULL, cmd, NULL, NULL, FALSE,
                  CREATE_NO_WINDOW | CREATE_SUSPENDED | CREATE_BREAKAWAY_FROM_JOB,
                  NULL, NULL, &si, &pi))
    {
        AssignProcessToJobObject(job, pi.hProcess);
        ResumeThread(pi.hThread);
    }
#endif // _WIN32
    Glib::set_application_name(PACKAGE_NAME);
}

Application& Application::get_instance()
{
    static Application i;
    return i;
}

MainWindow* Application::create_window()
{
    Glib::RefPtr<Gtk::Builder> builder = Gtk::Builder::create();

    try
    {
        builder->add_from_resource("/ui/ahoviewer.ui");
    }
    catch (const Glib::Error &ex)
    {
        std::cerr << "Gtk::Builder::add_from_resource: " << ex.what() << std::endl;
        return nullptr;
    }

    MainWindow *w = nullptr;
    builder->get_widget_derived("MainWindow", w);

    if (!w)
        throw std::runtime_error("Failed to create window");

    add_window(w);

    return w;
}

int Application::run(int argc, char **argv)
{
    register_application();

    if (!is_remote())
    {
        gtk_init(&argc, &argv);
#ifdef HAVE_GSTREAMER
        gst_init(&argc, &argv);
#endif // HAVE_GSTREAMER
    }

    return Gio::Application::run(argc, argv);
}

// Finds the first window with no local image list or creates a
// new window and then opens the file
void Application::on_open(const std::vector<Glib::RefPtr<Gio::File>> &f,
                          const Glib::ustring&)
{
    auto it = std::find_if(m_Windows.cbegin(), m_Windows.cend(),
        [](MainWindow *w){ return w->m_LocalImageList->empty(); });

    if (m_Windows.size() == 0 || it == m_Windows.cend())
    {
        auto w = create_window();
        w->open_file(f.front()->get_path());
    }
    else
    {
        (*it)->open_file(f.front()->get_path());
    }
}

void Application::on_startup()
{
    LIBXML_TEST_VERSION

    curl_global_init(CURL_GLOBAL_DEFAULT);

    curl_version_info_data *ver_info = curl_version_info(CURLVERSION_NOW);
    std::string ssl_lib = ver_info->ssl_version;

#if defined(USE_OPENSSL) && (OPENSSL_VERSION_NUMBER < 0x10100000L)
    if (ssl_lib.find("OpenSSL") != std::string::npos)
        init_openssl_locks();
#endif // defined(USE_OPENSSL) && (OPENSSL_VERSION_NUMBER < 0x10100000L)

#if defined(USE_GNUTLS) && GCRYPT_VERSION_NUMBER < 0x010600
    if (ssl_lib.find("GnuTLS") != std::string::npos)
        init_gnutls_locks();
#endif // defined(USE_GNUTLS) && GCRYPT_VERSION_NUMBER < 0x010600

    Gtk::Main::init_gtkmm_internals();

    // Get rid of the stupid dispatcher warnings from glibmm
    // That happen when cancelling downloads
    g_log_set_handler("glibmm", G_LOG_LEVEL_WARNING, glibmm_log_filter, NULL);

    Gio::Application::on_startup();
}

void Application::on_activate()
{
    auto w = create_window();

    if (m_Windows.size() == 1)
        w->restore_last_file();

    Gio::Application::on_activate();
}

void Application::add_window(MainWindow* w)
{
    if (m_Windows.size() == 0)
        w->m_OriginalWindow = true;

    m_Windows.push_back(w);
    w->signal_hide().connect(
        sigc::bind(sigc::mem_fun(*this, &Application::remove_window), w));

    hold();
}

void Application::remove_window(MainWindow* w)
{
    m_Windows.erase(
        std::remove(m_Windows.begin(), m_Windows.end(), w), m_Windows.end());
    delete w;

    if (m_Windows.size() == 1)
        m_Windows.front()->m_OriginalWindow = true;

    release();
}
