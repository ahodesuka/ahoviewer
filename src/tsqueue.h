#pragma once

#include <mutex>
#include <queue>

namespace AhoViewer
{
    template<typename T>
    class TSQueue
    {
    public:
        void push(const T& v)
        {
            std::scoped_lock lock{ m_Mutex };
            m_Queue.push(std::move(v));
        }
        void push(T&& v)
        {
            std::scoped_lock lock{ m_Mutex };
            m_Queue.push(std::move(v));
        }
        template<typename... Args>
        void emplace(Args&&... v)
        {
            std::scoped_lock lock{ m_Mutex };
            m_Queue.emplace(std::forward<Args>(v)...);
        }
        bool pop(T& v)
        {
            std::scoped_lock lock{ m_Mutex };
            if (m_Queue.empty())
                return false;
            v = std::move(m_Queue.front());
            m_Queue.pop();
            return true;
        }
        void clear()
        {
            std::scoped_lock lock{ m_Mutex };
            while (!m_Queue.empty())
                m_Queue.pop();
        }
        bool empty() const
        {
            std::scoped_lock lock{ m_Mutex };
            return m_Queue.empty();
        }
        size_t size() const
        {
            std::scoped_lock lock{ m_Mutex };
            return m_Queue.size();
        }

    private:
        std::queue<T> m_Queue;
        mutable std::mutex m_Mutex;
    };
}
