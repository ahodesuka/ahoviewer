#include <cstring>
#include <fstream>

#include "curler.h"
using namespace AhoViewer::Booru;

size_t Curler::write_cb(const unsigned char *ptr, size_t size, size_t nmemb, void *userp)
{
    Curler *self = static_cast<Curler*>(userp);
    size_t len = size * nmemb;

    self->m_Buffer.insert(self->m_Buffer.end(), ptr, ptr + len);
    self->m_SignalWrite(ptr, len);

    self->m_ProgressLock.writer_lock();
    if (self->m_DownloadTotal == 0)
    {
        double s;
        curl_easy_getinfo(self->m_EasyHandle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &s);
        self->m_DownloadTotal = s;
    }

    self->m_DownloadCurrent = self->m_Buffer.size();
    self->m_ProgressLock.writer_unlock();
    self->m_SignalProgress();

    return len;
}

Curler::Curler()
  : m_EasyHandle(curl_easy_init()),
    m_DownloadTotal(0),
    m_DownloadCurrent(0)
{
    init();
}

Curler::Curler(const std::string &url)
  : m_EasyHandle(curl_easy_init()),
    m_DownloadTotal(0),
    m_DownloadCurrent(0)
{
    init();
    set_url(url);
}

Curler::~Curler()
{
    curl_easy_cleanup(m_EasyHandle);
}

void Curler::init()
{
    curl_easy_setopt(m_EasyHandle, CURLOPT_WRITEFUNCTION, &Curler::write_cb);
    curl_easy_setopt(m_EasyHandle, CURLOPT_WRITEDATA, this);
    curl_easy_setopt(m_EasyHandle, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(m_EasyHandle, CURLOPT_MAXREDIRS, 5);
    curl_easy_setopt(m_EasyHandle, CURLOPT_FAILONERROR, 1);
    curl_easy_setopt(m_EasyHandle, CURLOPT_VERBOSE, 0);
    curl_easy_setopt(m_EasyHandle, CURLOPT_CONNECTTIMEOUT, 10);
    curl_easy_setopt(m_EasyHandle, CURLOPT_NOSIGNAL, 1);
}

void Curler::set_url(const std::string &url)
{
    m_Url = url;
    curl_easy_setopt(m_EasyHandle, CURLOPT_URL, m_Url.c_str());
}

std::string Curler::escape(const std::string &str) const
{
    return curl_easy_escape(m_EasyHandle, str.c_str(), 0);
}

bool Curler::perform()
{
    m_Buffer.clear();
    m_Response = curl_easy_perform(m_EasyHandle);

    return m_Response == CURLE_OK;
}

void Curler::get_progress(double &current, double &total)
{
    m_ProgressLock.reader_lock();
    current = m_DownloadCurrent;
    total   = m_DownloadTotal;
    m_ProgressLock.reader_unlock();
}

void Curler::save_file(const std::string &path) const
{
    std::ofstream ofs(path, std::ofstream::binary);

    if (ofs)
        std::copy(m_Buffer.begin(), m_Buffer.end(), std::ostream_iterator<unsigned char>(ofs));
}
