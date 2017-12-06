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
        void push(const T& v)
        {
            std::unique_lock<std::mutex> lock(m_Mutex);
            m_Queue.push(std::move(v));
        }
        void push(T&& v)
        {
            std::unique_lock<std::mutex> lock(m_Mutex);
            m_Queue.push(std::move(v));
        }
        template <typename... Args>
        void emplace(Args&&... v)
        {
            std::unique_lock<std::mutex> lock(m_Mutex);
            m_Queue.emplace(std::forward<Args>(v)...);
        }
        bool pop(T& v)
        {
            std::unique_lock<std::mutex> lock(m_Mutex);
            if (m_Queue.empty())
                return false;
            v = std::move(m_Queue.front());
            m_Queue.pop();
            return true;
        }
        void clear()
        {
            std::unique_lock<std::mutex> lock(m_Mutex);
            while (!m_Queue.empty()) m_Queue.pop();
        }
        bool empty() const
        {
            std::unique_lock<std::mutex> lock(m_Mutex);
            return m_Queue.empty();
        }

    private:
        std::queue<T> m_Queue;
        mutable std::mutex m_Mutex;
    };
}

#endif /* _TSQUEUE_H_ */
