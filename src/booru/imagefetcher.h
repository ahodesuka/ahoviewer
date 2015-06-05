#ifndef _IMAGEFETCHER_H_
#define _IMAGEFETCHER_H_

#include "curler.h"

namespace AhoViewer
{
    namespace Booru
    {
        /**
         * This is not a thread safe class.
         * All calls to add/remove_handle should be made from the same thread.
         **/
        class ImageFetcher : public sigc::trackable
        {
        public:
            ImageFetcher();
            virtual ~ImageFetcher();

            void add_handle(Curler *curler);
            void remove_handle(Curler *curler);
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

            bool event_cb(curl_socket_t sockfd, Glib::IOCondition cond);
            bool timeout_cb();
            void read_info();

            Glib::RefPtr<Glib::MainContext> m_MainContext;
            Glib::RefPtr<Glib::MainLoop> m_MainLoop;
            Glib::Threads::Thread *m_Thread;

            CURLM *m_MultiHandle;
            int m_RunningHandles;

            sigc::connection m_TimeoutConn;
        };
    }
}

#endif /* _IMAGEFETCHER_H_ */
