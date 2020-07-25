#include "imagefetcher.h"
using namespace AhoViewer::Booru;

int ImageFetcher::socket_cb(CURL*, curl_socket_t s, int action, void *userp, void *sockp)
{
    ImageFetcher *self = static_cast<ImageFetcher*>(userp);
    SockInfo *fdp = static_cast<SockInfo*>(sockp);

    if (action == CURL_POLL_REMOVE)
    {
        if (fdp)
            delete fdp;
    }
    else
    {
        bool need_assign = false;

        if (!fdp)
        {
            need_assign = true;
            fdp = new SockInfo();
            fdp->chan = Glib::IOChannel::create_from_fd(s);
        }

        Glib::IOCondition kind = static_cast<Glib::IOCondition>(
            ((action & CURL_POLL_IN) ? Glib::IO_IN : 0) |
            ((action & CURL_POLL_OUT) ? Glib::IO_OUT : 0));

        Glib::RefPtr<Glib::IOSource> source = fdp->chan->create_watch(kind);
        fdp->sockfd = s;

        if (fdp->conn)
            fdp->conn.disconnect();

        fdp->conn = source->connect(sigc::bind<0>(sigc::mem_fun(self, &ImageFetcher::event_cb), s));
        source->attach(self->m_MainContext);

        if (need_assign)
            curl_multi_assign(self->m_MultiHandle, s, fdp);
    }

    return 0;
}

int ImageFetcher::timer_cb(CURLM*, long timeout_ms, void *userp)
{
    ImageFetcher *self = static_cast<ImageFetcher*>(userp);

    if (timeout_ms > 0)
        self->m_TimeoutConn = self->m_MainContext->signal_timeout().connect(
                sigc::mem_fun(self, &ImageFetcher::timeout_cb), timeout_ms);
    else if (timeout_ms == 0)
        self->timeout_cb();
    else if (timeout_ms == -1 && self->m_TimeoutConn)
        self->m_TimeoutConn.disconnect();

    return 0;
}

ImageFetcher::ImageFetcher(const int max_cons)
  : m_MainContext(Glib::MainContext::create()),
    m_MainLoop(Glib::MainLoop::create(m_MainContext)),
    m_MultiHandle(curl_multi_init())
{
    m_Thread = std::thread([&]()
    {
        auto added_dis   = std::make_shared<Glib::Dispatcher>(m_MainContext),
             unpause_dis = std::make_shared<Glib::Dispatcher>(m_MainContext);

        added_dis->connect(sigc::mem_fun(*this, &ImageFetcher::on_handle_added));
        unpause_dis->connect(sigc::mem_fun(*this, &ImageFetcher::on_handle_unpause));

        m_SignalHandleAdded   = added_dis;
        m_SignalHandleUnpause = unpause_dis;

        m_MainLoop->run();
    });

    curl_multi_setopt(m_MultiHandle, CURLMOPT_SOCKETFUNCTION, &ImageFetcher::socket_cb);
    curl_multi_setopt(m_MultiHandle, CURLMOPT_SOCKETDATA, this);
    curl_multi_setopt(m_MultiHandle, CURLMOPT_TIMERFUNCTION, &ImageFetcher::timer_cb);
    curl_multi_setopt(m_MultiHandle, CURLMOPT_TIMERDATA, this);
    curl_multi_setopt(m_MultiHandle, CURLMOPT_MAX_HOST_CONNECTIONS, max_cons);
    // Enable HTTP/2 multiplexing (this is on by default with curl >=7.62.0)
    curl_multi_setopt(m_MultiHandle, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX);
}

ImageFetcher::~ImageFetcher()
{
    if (!m_Shutdown)
        shutdown();
    curl_multi_cleanup(m_MultiHandle);
}

// This is used in the Booru::Page destructor in order to
// prevent any undefined behavior between the ImageList and this
// while destructing each.
void ImageFetcher::shutdown()
{
    m_Shutdown = true;
    if (m_TimeoutConn)
        m_TimeoutConn.disconnect();

    m_MainLoop->quit();
    if (m_Thread.joinable())
        m_Thread.join();

    for (Curler *c : m_Curlers)
        remove_handle(c);
}

void ImageFetcher::add_handle(Curler *curler)
{
    if (m_Shutdown)
        return;

    curler->m_Cancel->reset();
    curler->clear();
    curler->m_Active = true;
    m_CurlerQueue.push(curler);

    if (auto dis = m_SignalHandleAdded.lock())
        dis->emit();
}

void ImageFetcher::unpause_handle(Curler *curler)
{
    if (m_Shutdown)
        return;

    m_CurlerUnpauseQueue.push(curler);

    if (auto dis = m_SignalHandleUnpause.lock())
        dis->emit();
}

void ImageFetcher::on_handle_added()
{
    Curler *curler = nullptr;
    while (!m_Shutdown && m_CurlerQueue.pop(curler))
    {
        m_Curlers.push_back(curler);
        curler->set_imagefetcher(this);
        curler->m_DownloadCurrent = curler->m_DownloadTotal = 0;
        curler->m_StartTime = std::chrono::steady_clock::now();
        curl_multi_add_handle(m_MultiHandle, curler->m_EasyHandle);
    }
}

void ImageFetcher::on_handle_unpause()
{
    Curler *curler = nullptr;
    while (!m_Shutdown && m_CurlerUnpauseQueue.pop(curler))
    {
        curler->m_Pause = false;
        curl_easy_pause(curler->m_EasyHandle, CURLPAUSE_CONT);
    }
}

void ImageFetcher::remove_handle(Curler *curler)
{
    if (!curler)
        return;

    curl_multi_remove_handle(m_MultiHandle, curler->m_EasyHandle);
    m_Curlers.erase(std::remove(m_Curlers.begin(), m_Curlers.end(), curler), m_Curlers.end());

    curler->m_Active = false;
    curler->set_imagefetcher(nullptr);
}

bool ImageFetcher::event_cb(curl_socket_t sockfd, Glib::IOCondition cond)
{
    int action = (cond & Glib::IO_IN ? CURL_CSELECT_IN : 0) |
                 (cond & Glib::IO_OUT ? CURL_CSELECT_OUT : 0);

    curl_multi_socket_action(m_MultiHandle, sockfd, action, &m_RunningHandles);
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

    while ((msg = curl_multi_info_read(m_MultiHandle, &msgs)))
    {
        if (msg->msg == CURLMSG_DONE)
        {
            Curler *curler = nullptr;
            curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &curler);

            if (curler)
            {
                remove_handle(curler);
                curler->m_Response = msg->data.result;

                if (!curler->is_cancelled())
                    curler->m_SignalFinished();
                else
                    curler->clear();
            }
        }
    }
}
