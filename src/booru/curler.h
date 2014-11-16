#ifndef _CURLER_H_
#define _CURLER_H_

#include <curl/curl.h>
#include <glibmm.h>
#include <chrono>
#include <string>
#include <vector>

namespace AhoViewer
{
    namespace Booru
    {
        class Curler
        {
        public:
            friend class ImageFetcher;

            typedef std::chrono::time_point<std::chrono::steady_clock> time_point_t;
            typedef sigc::signal<void, const unsigned char*, size_t> SignalWriteType;

            Curler();
            Curler(const std::string &url);
            ~Curler();

            void set_url(const std::string &url);
            std::string escape(const std::string &str) const;
            bool perform();

            void clear() { m_Buffer.clear(); }
            void save_file(const std::string &path) const;

            void get_progress(double &current, double &total);

            bool is_active() const { return m_Active; }
            unsigned char* get_data() { return m_Buffer.data(); }
            size_t get_data_size() const { return m_Buffer.size(); }
            std::string get_error() const  { return curl_easy_strerror(m_Response); }
            time_point_t get_start_time() const { return m_StartTime; }

            SignalWriteType signal_write() const { return m_SignalWrite; }
            Glib::Dispatcher& signal_progress() { return m_SignalProgress; }
            Glib::Dispatcher& signal_finished() { return m_SignalFinished; }
        private:
            static size_t write_cb(const unsigned char *ptr, size_t size, size_t nmemb, void *userp);

            void init();

            CURL *m_EasyHandle;
            CURLcode m_Response;
            std::string m_Url;
            std::vector<unsigned char> m_Buffer;

            bool m_Active;
            time_point_t m_StartTime;

            Glib::Threads::RWLock m_ProgressLock;
            double m_DownloadTotal,
                   m_DownloadCurrent;

            SignalWriteType m_SignalWrite;
            Glib::Dispatcher m_SignalProgress,
                             m_SignalFinished;
        };
    }
}

#endif /* _CURLER_H_ */
