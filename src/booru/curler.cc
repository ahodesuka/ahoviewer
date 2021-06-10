#include "curler.h"
using namespace AhoViewer::Booru;

#include "imagefetcher.h"

// Used for looking closer at what libcurl is doing
// set it to 1 with CXXFLAGS and prepare to be spammed
#ifndef VERBOSE_LIBCURL
#define VERBOSE_LIBCURL 0
#endif

const char* Curler::UserAgent
{
#if defined(__linux__)
    "Mozilla/5.0 (Linux x86_64; rv:89.0) Gecko/20100101 Firefox/89.0"
#elif defined(__APPLE__)
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 11.4; rv:89.0) Gecko/20100101 Firefox/89.0"
#elif defined(_WIN32)
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:89.0) Gecko/20100101 Firefox/89.0"
#else
    "Mozilla/5.0"
#endif
};

size_t Curler::write_cb(const unsigned char* ptr, size_t size, size_t nmemb, void* userp)
{
    Curler* self{ static_cast<Curler*>(userp) };

    if (self->is_cancelled())
        return 0;

    if (self->m_DownloadTotal == 0)
    {
        curl_off_t s;
        curl_easy_getinfo(self->m_EasyHandle, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &s);
        self->m_DownloadTotal = s;
    }

    if (self->m_Pause)
        return CURL_WRITEFUNC_PAUSE;

    size_t len{ size * nmemb };

    self->m_Buffer.insert(self->m_Buffer.end(), ptr, ptr + len);
    self->m_SignalWrite(ptr, len);

    self->m_DownloadCurrent = self->m_Buffer.size();

    if (!self->is_cancelled())
        self->m_SignalProgress();

    return len;
}

int Curler::progress_cb(void* userp, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
{
    Curler* self{ static_cast<Curler*>(userp) };
    return self->is_cancelled();
}

Curler::Curler(const std::string& url, CURLSH* share)
    : m_EasyHandle(curl_easy_init()),
      m_Cancel(Gio::Cancellable::create())
{
    curl_easy_setopt(m_EasyHandle, CURLOPT_USERAGENT, UserAgent);
    // This will fall back to 1.1 if the server does not support HTTP 2
    // Since curl 7.62.0 the default value is CURL_HTTP_VERSION_2TLS, but
    // explicitly setting it here enables it for non https requests as well
    curl_easy_setopt(m_EasyHandle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2);
    curl_easy_setopt(m_EasyHandle, CURLOPT_WRITEFUNCTION, &Curler::write_cb);
    curl_easy_setopt(m_EasyHandle, CURLOPT_WRITEDATA, this);
    curl_easy_setopt(m_EasyHandle, CURLOPT_XFERINFOFUNCTION, &Curler::progress_cb);
    curl_easy_setopt(m_EasyHandle, CURLOPT_XFERINFODATA, this);
    curl_easy_setopt(m_EasyHandle, CURLOPT_NOPROGRESS, 0);
    curl_easy_setopt(m_EasyHandle, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(m_EasyHandle, CURLOPT_MAXREDIRS, 5);
    curl_easy_setopt(m_EasyHandle, CURLOPT_VERBOSE, VERBOSE_LIBCURL);
    curl_easy_setopt(m_EasyHandle, CURLOPT_CONNECTTIMEOUT, 60);
    curl_easy_setopt(m_EasyHandle, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(m_EasyHandle, CURLOPT_PRIVATE, this);

#ifdef _WIN32
    gchar* g{ g_win32_get_package_installation_directory_of_module(NULL) };
    if (g)
    {
        std::string cert_path{ Glib::build_filename(g, "ca-bundle.crt") };
        curl_easy_setopt(m_EasyHandle, CURLOPT_CAINFO, cert_path.c_str());
        g_free(g);
    }
#endif // _WIN32

    if (!url.empty())
        set_url(url);

    if (share)
        set_share_handle(share);
}

Curler::~Curler()
{
    curl_easy_cleanup(m_EasyHandle);
}

void Curler::set_url(std::string url)
{
    m_Url = std::move(url);
    curl_easy_setopt(m_EasyHandle, CURLOPT_URL, m_Url.c_str());
}

void Curler::set_follow_location(const bool n) const
{
    curl_easy_setopt(m_EasyHandle, CURLOPT_FOLLOWLOCATION, n);
}

void Curler::set_referer(const std::string& url) const
{
    curl_easy_setopt(m_EasyHandle, CURLOPT_REFERER, url.c_str());
}

void Curler::set_http_auth(const std::string& u, const std::string& p) const
{
    if (!u.empty())
    {
        curl_easy_setopt(m_EasyHandle, CURLOPT_USERNAME, u.c_str());
        curl_easy_setopt(m_EasyHandle, CURLOPT_PASSWORD, p.c_str());
    }
}

void Curler::set_cookie_jar(const std::string& path) const
{
    curl_easy_setopt(m_EasyHandle, CURLOPT_COOKIEJAR, path.c_str());
}

void Curler::set_cookie_file(const std::string& path) const
{
    curl_easy_setopt(m_EasyHandle, CURLOPT_COOKIEFILE, path.c_str());
}

void Curler::set_post_fields(const std::string& fields) const
{
    curl_easy_setopt(m_EasyHandle, CURLOPT_POSTFIELDSIZE, fields.length());
    curl_easy_setopt(m_EasyHandle, CURLOPT_POSTFIELDS, fields.c_str());
}

void Curler::set_share_handle(CURLSH* s) const
{
    curl_easy_setopt(m_EasyHandle, CURLOPT_SHARE, s);
}

std::string Curler::escape(const std::string& str) const
{
    std::string r;
    char* s{ curl_easy_escape(m_EasyHandle, str.c_str(), str.length()) };

    if (s)
    {
        r = s;
        curl_free(s);
    }

    return r;
}

bool Curler::perform()
{
    m_Cancel->reset();
    clear();

    return curl_easy_perform(m_EasyHandle) == CURLE_OK;
}

void Curler::get_progress(curl_off_t& current, curl_off_t& total)
{
    current = m_DownloadCurrent;
    total   = m_DownloadTotal;
}

long Curler::get_response_code() const
{
    long c;
    curl_easy_getinfo(m_EasyHandle, CURLINFO_RESPONSE_CODE, &c);

    return c;
}

void Curler::pause()
{
    m_Pause = true;
}

void Curler::unpause()
{
    if (m_ImageFetcher)
    {
        m_ImageFetcher->unpause_handle(this);
    }
    else
    {
        m_Pause = false;
        curl_easy_pause(m_EasyHandle, CURLPAUSE_CONT);
    }
}

void Curler::save_file(const std::string& path) const
{
    std::string etag;
    Glib::RefPtr<Gio::File> f{ Gio::File::create_for_path(path) };
    f->replace_contents(reinterpret_cast<const char*>(m_Buffer.data()), m_Buffer.size(), "", etag);
}

void Curler::save_file_async(const std::string& path, const Gio::SlotAsyncReady& cb)
{
    Glib::RefPtr<Gio::File> f{ Gio::File::create_for_path(path) };
    f->replace_contents_async(
        cb, m_Cancel, reinterpret_cast<const char*>(m_Buffer.data()), m_Buffer.size(), "");
}

void Curler::save_file_finish(const Glib::RefPtr<Gio::AsyncResult>& r)
{
    Glib::RefPtr<Gio::File> f{ Glib::RefPtr<Gio::File>::cast_dynamic(r->get_source_object_base()) };
    clear();

    if (f)
        f->replace_contents_finish(r);
}
