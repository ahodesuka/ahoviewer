#ifndef _TSQUEUE_H_
#define _TSQUEUE_H_

#include <mutex>
#include <queue>

namespace AhoViewer
{
    template <typename T>
    class TSQueue
    {
    public:
        bool push(T v)
        {
            std::unique_lock<std::mutex> lock(m_Mutex);
            m_Queue.push(v);
            return true;
        }
        bool pop(T &v)
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
}

#endif /* _TSQUEUE_H_ */
