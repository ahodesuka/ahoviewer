#include <gtkmm.h>
#include <cstdlib>
#include <iostream>
#include <curl/curl.h>
#include <libxml/parser.h>

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
    gcry_control(GCRYCTL_SET_THREAD_CBS);
}
#endif // GCRYPT_VERSION_NUMBER < 0x010600
#endif // USE_GNUTLS

#ifdef HAVE_GSTREAMER
#include <gst/gst.h>
#endif // HAVE_GSTREAMER

extern const unsigned char ahoviewer_ui[];
extern const unsigned long ahoviewer_ui_size;

int main(int argc, char **argv)
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

#ifdef _WIN32
    argv = g_win32_get_command_line();
#endif // _WIN32

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
    catch (const Glib::Error &ex)
    {
        std::cerr << "Gtk::Builder::add_from_string: " << ex.what() << std::endl;
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

#ifdef _WIN32
    g_strfreev(argv);
#endif // _WIN32

    main.run(*window);
    curl_global_cleanup();
    xmlCleanupParser();

    return EXIT_SUCCESS;
}
