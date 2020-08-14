#pragma once

/*********************************************************
 *
 *  Copyright (C) 2014 by Vitaliy Vitsentiy
 *  https://github.com/vit-vit/CTPL
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *
 *  October 2015, Jonathan Hadida:
 *   - rename, reformat and comment some functions
 *   - add a restart function
 *  	- fix a few unsafe spots, eg:
 *  		+ order of testing in setup_thread;
 *  		+ atomic guards on pushes;
 *  		+ make clear_queue private
 *
 *  November 2017, ahoka
 *   - Removed thread id from function args
 *   - Made threads persistent until pool is destructed or resized.
 *
 *********************************************************/

#include "tsqueue.h"

#include <algorithm>
#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

// thread pool to run user's functors with signature
//      ret func(...)
// ret is some return type

namespace AhoViewer
{
    class ThreadPool
    {
    public:
        ThreadPool(size_t n_threads = std::thread::hardware_concurrency() - 1)
        {
            n_threads = std::max(n_threads, size_t{ 1 });
            resize(n_threads);
        }

        // the destructor waits for all tasks in the queue to be finished
        ~ThreadPool()
        {
            m_Shutdown = true;
            m_Cond.notify_all(); // stop all waiting threads
            for (auto& t : m_Threads)
            {
                if (t->joinable())
                    t->join();
            }
        }

        // get the number of running threads in the pool
        inline size_t size() const { return m_Threads.size(); }

        // number of idle threads
        inline size_t n_idle() const { return m_NumIdle; }

        // whether there are any threads
        inline bool active() const { return m_NumIdle != m_Threads.size() || !m_Queue.empty(); }

        // get a specific thread
        inline std::thread& get_thread(size_t i) { return *m_Threads.at(i); }

        // change the number of threads in the pool
        // should be called from one thread, otherwise be careful to not interleave, also with
        // interrupt()
        void resize(size_t n_threads)
        {
            if (n_threads == 0)
                throw std::runtime_error("ThreadPool::resize with size 0 is not allowed");

            if (!m_Interrupt && !m_Kill)
            {
                size_t old_n_threads = m_Threads.size();

                // if the number of threads is increased
                if (old_n_threads <= n_threads)
                {
                    m_Threads.resize(n_threads);
                    m_Abort.resize(n_threads);

                    for (size_t i = old_n_threads; i < n_threads; ++i)
                    {
                        m_Abort[i] = std::make_shared<std::atomic<bool>>(false);
                        setup_thread(i);
                    }
                }
                // the number of threads is decreased
                else
                {
                    for (size_t i = old_n_threads - 1; i >= n_threads; --i)
                    {
                        *m_Abort[i] = true; // this thread will finish
                        m_Threads[i]->detach();
                    }

                    // stop the detached threads that were waiting
                    m_Cond.notify_all();

                    m_Threads.resize(n_threads); // safe to delete because the threads are detached
                    m_Abort.resize(n_threads); // safe to delete because the threads have copies of
                                               // shared_ptr of the flags, not originals
                }
            }
        }

        // Removes remaining tasks and resets the pool back to it's initial state
        void kill()
        {
            interrupt(true);
            init();
        }

        // Waits for remaining tasks to complete and resets the pool back to it's initial state
        void wait()
        {
            interrupt();
            init();
        }

        template<typename F, typename... Args>
        decltype(auto) push(F&& f, Args&&... args)
        {
            using return_type = typename std::result_of<F(Args...)>::type;
            if (!m_Interrupt && !m_Kill)
            {
                auto task{ std::make_shared<std::packaged_task<return_type()>>(
                    std::bind(std::forward<F>(f), std::forward<Args>(args)...)) };
                auto func{ std::make_unique<std::function<void()>>([task]() { (*task)(); }) };

                m_Queue.push(std::move(func));
                m_Cond.notify_one();
                return task->get_future();
            }
            else
                return std::future<return_type>();
        }

    private:
        // delete copy and move constructors
        ThreadPool(const ThreadPool&) = delete;
        ThreadPool(ThreadPool&&)      = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;
        ThreadPool& operator=(ThreadPool&&) = delete;

        // wait for all computing threads to finish
        // may be called asynchronously to not pause the calling thread while waiting
        // if kill == true, queued tasks are not completed and removed
        void interrupt(bool kill = false)
        {
            if (kill)
            {
                if (m_Kill)
                    return;
                m_Kill = true;
                m_Queue.clear();
            }
            else
            {
                if (m_Interrupt || m_Kill)
                    return;
                m_Interrupt = true; // give the waiting threads a command to finish
            }

            m_Cond.notify_all(); // stop all waiting threads
            // wait for the computing threads to finish
            // if m_Shutdown is set the destructor will wait for the queue to finish
            std::unique_lock<std::mutex> lock(m_InterruptMutex);
            m_Icond.wait(lock, [&]() {
                return (m_NumIdle == m_Threads.size() && m_Queue.empty()) || m_Shutdown;
            });
        }

        // init all flags
        void init()
        {
            m_Kill      = false;
            m_Interrupt = false;
            m_Shutdown  = false;
        }

        // each thread pops jobs from the queue until:
        //  - its abort flag is set (used when resizing the pool)
        //  - a global shutdown is set, then only idle threads terminate
        void setup_thread(size_t i)
        {
            // a copy of the shared ptr to the abort
            std::shared_ptr<std::atomic<bool>> abort_ptr(m_Abort[i]);

            auto f = [&, abort_ptr]() {
                std::atomic<bool>& abort{ *abort_ptr };
                std::unique_ptr<std::function<void()>> func{ nullptr };
                bool more_tasks = m_Queue.pop(func);

                while (true)
                {
                    // if there is anything in the queue
                    while (more_tasks)
                    {
                        (*func)();
                        if (abort)
                            return; // return even if the queue is not empty yet
                        else
                            more_tasks = m_Queue.pop(func);
                    }

                    ++m_NumIdle;
                    m_Icond.notify_one();
                    // the queue is empty here, wait for the next command
                    {
                        std::unique_lock<std::mutex> lock(m_Mutex);
                        m_Cond.wait(lock, [&]() {
                            more_tasks = m_Queue.pop(func);
                            return abort || m_Shutdown || more_tasks;
                        });
                    }
                    --m_NumIdle;
                    if (!more_tasks && (m_Shutdown || abort))
                        return;
                }
            };

            m_Threads[i] = std::make_unique<std::thread>(f);
        }

        std::vector<std::unique_ptr<std::thread>> m_Threads;
        std::vector<std::shared_ptr<std::atomic<bool>>> m_Abort;
        TSQueue<std::unique_ptr<std::function<void()>>> m_Queue;

        std::atomic<bool> m_Interrupt{ false }, m_Kill{ false }, m_Shutdown{ false };
        std::atomic<size_t> m_NumIdle{ 0 };

        std::mutex m_Mutex, m_InterruptMutex;
        std::condition_variable m_Cond, m_Icond;
    };
}
