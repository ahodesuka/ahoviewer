#pragma once

#include "curler.h"
#include "tsqueue.h"

#include <thread>

namespace AhoViewer::Booru
{
    class ImageFetcher : public sigc::trackable
    {
    public:
        ImageFetcher(const bool multiplex);
        virtual ~ImageFetcher();

        void shutdown();

        void add_handle(Curler* curler);
        void unpause_handle(Curler* curler);

    private:
        struct SockInfo
        {
            ~SockInfo()
            {
                if (conn)
                    conn.disconnect();
            }

            curl_socket_t sockfd;
            Glib::RefPtr<Glib::IOChannel> chan;
            sigc::connection conn;
        };

        static int socket_cb(CURL*, curl_socket_t s, int action, void* userp, void* sockp);
        static int timer_cb(CURLM*, long timeout_ms, void* userp);

        void on_handle_added();
        void on_handle_unpause();
        void remove_handle(Curler* curler);
        bool event_cb(curl_socket_t sockfd, Glib::IOCondition cond);
        bool timeout_cb();
        void read_info();

        Glib::RefPtr<Glib::MainContext> m_MainContext;
        Glib::RefPtr<Glib::MainLoop> m_MainLoop;
        std::thread m_Thread;

        CURLM* m_MultiHandle;
        int m_RunningHandles{ 0 };
        TSQueue<Curler*> m_CurlerQueue, m_CurlerUnpauseQueue;
        std::vector<Curler*> m_Curlers;

        std::weak_ptr<Glib::Dispatcher> m_SignalHandleAdded, m_SignalHandleUnpause;

        std::atomic<bool> m_Shutdown{ false };

        sigc::connection m_TimeoutConn;
    };
}
