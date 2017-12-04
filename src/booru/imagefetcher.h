#ifndef _IMAGEFETCHER_H_
#define _IMAGEFETCHER_H_

#include "curler.h"

#include <mutex>
#include <queue>
#include <thread>

namespace AhoViewer
{
    namespace Booru
    {
        class ImageFetcher : public sigc::trackable
        {
            // Thread-safe queue {{{
            template <typename T>
            class Queue
            {
            public:
                bool push(T value)
                {
                    std::unique_lock<std::mutex> lock(m_Mutex);
                    m_Queue.push(value);
                    return true;
                }
                bool pop(T & v)
                {
                    std::unique_lock<std::mutex> lock(m_Mutex);
                    if (m_Queue.empty())
                        return false;
                    v = m_Queue.front();
                    m_Queue.pop();
                    return true;
                }
                void clear()
                {
                    std::unique_lock<std::mutex> lock(m_Mutex);
                    while (!m_Queue.empty()) m_Queue.pop();
                }

            private:
                std::queue<T> m_Queue;
                std::mutex m_Mutex;
            };
            // }}}
        public:
            ImageFetcher();
            virtual ~ImageFetcher();

            void shutdown();

            void add_handle(Curler *curler);

            void set_max_connections(unsigned int n);
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

            void on_handle_added();
            void remove_handle(Curler *curler);
            bool event_cb(curl_socket_t sockfd, Glib::IOCondition cond);
            bool timeout_cb();
            void read_info();

            Glib::RefPtr<Glib::MainContext> m_MainContext;
            Glib::RefPtr<Glib::MainLoop> m_MainLoop;
            std::thread m_Thread;
            std::mutex m_Mutex;

            CURLM *m_MultiHandle;
            int m_RunningHandles;
            Queue<Curler*> m_CurlerQueue;
            std::vector<Curler*> m_Curlers;

            std::weak_ptr<Glib::Dispatcher> m_SignalHandleAdded;

            std::atomic<bool> m_Shutdown;

            sigc::connection m_TimeoutConn;
        };
    }
}

#endif /* _IMAGEFETCHER_H_ */
