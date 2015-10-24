#ifndef _CURLER_H_
#define _CURLER_H_

#include <curl/curl.h>
#include <glibmm.h>
#include <giomm.h>
#include <atomic>
#include <chrono>
#include <string>
#include <vector>

namespace AhoViewer
{
    namespace Booru
    {
        typedef std::chrono::time_point<std::chrono::steady_clock> time_point_t;

        class Curler
        {
        public:
            friend class ImageFetcher;

            typedef sigc::signal<void, const unsigned char*, size_t> SignalWriteType;

            Curler(const std::string &url = "");
            ~Curler();

            void set_url(const std::string &url);
            void set_no_body(const bool n = true);
            void set_follow_location(const bool n = true);
            void set_referer(const std::string &url) const;
            std::string escape(const std::string &str) const;
            bool perform();

            void clear() { m_Buffer.clear(); }
            void save_file(const std::string &path) const;

            void get_progress(double &current, double &total);

            bool is_active() const { return m_Active; }
            std::string get_url() const { return m_Url; }

            unsigned char* get_data() { return m_Buffer.data(); }
            size_t get_data_size() const { return m_Buffer.size(); }

            std::string get_error() const  { return curl_easy_strerror(m_Response); }
            long get_response_code() const;
            time_point_t get_start_time() const { return m_StartTime; }

            void cancel() { m_Cancel->cancel(); }
            bool is_cancelled() const { return m_Cancel->is_cancelled(); }

            SignalWriteType signal_write() const { return m_SignalWrite; }
            Glib::Dispatcher& signal_progress() { return m_SignalProgress; }
            Glib::Dispatcher& signal_finished() { return m_SignalFinished; }
        private:
            static size_t write_cb(const unsigned char *ptr, size_t size, size_t nmemb, void *userp);
            static int progress_cb(void *userp, curl_off_t, curl_off_t, curl_off_t, curl_off_t);

            CURL *m_EasyHandle;
            CURLcode m_Response;
            std::string m_Url;
            std::vector<unsigned char> m_Buffer;

            bool m_Active;
            std::atomic<double> m_DownloadTotal,
                                m_DownloadCurrent;
            time_point_t m_StartTime;

            Glib::RefPtr<Gio::Cancellable> m_Cancel;

            SignalWriteType m_SignalWrite;
            Glib::Dispatcher m_SignalProgress,
                             m_SignalFinished;
        };
    }
}

#endif /* _CURLER_H_ */
