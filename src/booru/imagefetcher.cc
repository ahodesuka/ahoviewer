#include "imagefetcher.h"
using namespace AhoViewer::Booru;

int ImageFetcher::socket_cb(CURL*, curl_socket_t s, int action, void *userp, void *sockp)
{
    ImageFetcher *self = static_cast<ImageFetcher*>(userp);
    SockInfo *fdp = static_cast<SockInfo*>(sockp);

    if (action == CURL_POLL_REMOVE && fdp)
    {
        delete fdp;
    }
    else
    {
        if (!fdp)
        {
            fdp = new SockInfo();
            fdp->chan = Glib::IOChannel::create_from_fd(s);
            curl_multi_assign(self->m_MultiHandle, s, fdp);
        }

        Glib::IOCondition kind;
        if (action & CURL_POLL_IN)  kind |= Glib::IO_IN;
        if (action & CURL_POLL_OUT) kind |= Glib::IO_OUT;

        Glib::RefPtr<Glib::IOSource> source = fdp->chan->create_watch(kind);
        fdp->sockfd = s;

        if (fdp->conn)
            fdp->conn.disconnect();

        fdp->conn = source->connect(sigc::bind<0>(sigc::mem_fun(self, &ImageFetcher::event_cb), s));
        source->attach(self->m_MainContext);
    }

    return 0;
}

int ImageFetcher::timer_cb(CURLM*, long timeout_ms, void *userp)
{
    ImageFetcher *self = static_cast<ImageFetcher*>(userp);

    self->m_TimeoutConn = self->m_MainContext->signal_timeout().connect(
            sigc::mem_fun(self, &ImageFetcher::timeout_cb), timeout_ms);

    return 0;
}

ImageFetcher::ImageFetcher()
  : m_MainContext(Glib::MainContext::create()),
    m_MainLoop(Glib::MainLoop::create(m_MainContext)),
    m_Thread(nullptr),
    m_MultiHandle(curl_multi_init()),
    m_RunningHandles(0)
{
    m_Thread = Glib::Threads::Thread::create([ this ]() { m_MainLoop->run(); });

    curl_multi_setopt(m_MultiHandle, CURLMOPT_SOCKETFUNCTION, &ImageFetcher::socket_cb);
    curl_multi_setopt(m_MultiHandle, CURLMOPT_SOCKETDATA, this);
    curl_multi_setopt(m_MultiHandle, CURLMOPT_TIMERFUNCTION, &ImageFetcher::timer_cb);
    curl_multi_setopt(m_MultiHandle, CURLMOPT_TIMERDATA, this);
}

ImageFetcher::~ImageFetcher()
{
    m_MainLoop->quit();
    m_Thread->join();
    m_Thread = nullptr;

    for (Curler *c : m_Curlers)
        remove_handle(c);

    curl_multi_cleanup(m_MultiHandle);
}

void ImageFetcher::add_handle(Curler *curler)
{
    Glib::Threads::Mutex::Lock lock(m_Mutex);

    curl_easy_setopt(curler->m_EasyHandle, CURLOPT_PRIVATE, curler);
    m_Curlers.push_back(curler);
    curl_multi_add_handle(m_MultiHandle, curler->m_EasyHandle);

    curler->m_Cancel->reset();
    curler->m_Active = true;
    curler->m_StartTime = std::chrono::steady_clock::now();
}

void ImageFetcher::remove_handle(Curler *curler)
{
    Glib::Threads::Mutex::Lock lock(m_Mutex);

    curl_multi_remove_handle(m_MultiHandle, curler->m_EasyHandle);
    m_Curlers.erase(std::remove(m_Curlers.begin(), m_Curlers.end(), curler), m_Curlers.end());

    curler->m_Active = false;
}

bool ImageFetcher::event_cb(curl_socket_t sockfd, Glib::IOCondition cond)
{
    int action = (cond & Glib::IO_IN ? CURL_CSELECT_IN : 0) |
                 (cond & Glib::IO_OUT ? CURL_CSELECT_OUT : 0);

    {
        Glib::Threads::Mutex::Lock lock(m_Mutex);
        curl_multi_socket_action(m_MultiHandle, sockfd, action, &m_RunningHandles);
    }
    read_info();

    if (m_RunningHandles == 0)
    {
        m_TimeoutConn.disconnect();
        return false;
    }

    return true;
}

bool ImageFetcher::timeout_cb()
{
    curl_multi_socket_action(m_MultiHandle, CURL_SOCKET_TIMEOUT, 0, &m_RunningHandles);
    read_info();

    return false;
}

void ImageFetcher::read_info()
{
    int msgs;
    CURLMsg *msg = nullptr;
    Glib::Threads::Mutex::Lock lock(m_Mutex);

    while ((msg = curl_multi_info_read(m_MultiHandle, &msgs)))
    {
        if (msg->msg == CURLMSG_DONE)
        {
            Curler *curler = nullptr;
            curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &curler);

            if (curler)
            {
                lock.release();
                remove_handle(curler);
                lock.acquire();

                if (!curler->is_cancelled())
                    curler->m_SignalFinished();
            }
        }
    }
}
