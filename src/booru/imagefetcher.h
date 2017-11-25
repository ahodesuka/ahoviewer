#ifndef _IMAGEFETCHER_H_
#define _IMAGEFETCHER_H_

#include "curler.h"

#include <mutex>
#include <thread>

namespace AhoViewer
{
    namespace Booru
    {
        class ImageFetcher : public sigc::trackable
        {
        public:
            ImageFetcher();
            virtual ~ImageFetcher();

            void add_handle(Curler *curler);
        private:
            struct SockInfo
            {
                ~SockInfo() { if (conn) conn.disconnect(); }

                curl_socket_t sockfd;
                Glib::RefPtr<Glib::IOChannel> chan;
                sigc::connection conn;
            };

            static int socket_cb(CURL*, curl_socket_t s, int action, void *userp, void *sockp);
            static int timer_cb(CURLM*, long timeout_ms, void *userp);

            void remove_handle(Curler *curler);
            bool event_cb(curl_socket_t sockfd, Glib::IOCondition cond);
            bool timeout_cb();
            void read_info();

            Glib::RefPtr<Glib::MainContext> m_MainContext;
            Glib::RefPtr<Glib::MainLoop> m_MainLoop;
            std::thread m_Thread;
            std::recursive_mutex m_Mutex;

            CURLM *m_MultiHandle;
            int m_RunningHandles;
            std::vector<Curler*> m_Curlers;

            sigc::connection m_TimeoutConn;
        };
    }
}

#endif /* _IMAGEFETCHER_H_ */
