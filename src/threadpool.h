#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_

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

#include <functional>
#include <thread>
#include <atomic>
#include <vector>
#include <memory>
#include <future>
#include <mutex>
#include <queue>


// thread pool to run user's functors with signature
//      ret func(...)
// ret is some return type


namespace AhoViewer
{
    class ThreadPool
    {
        template <typename T>
        class Queue
        {
        public:
            bool push(T const & value)
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_queue.push(value);
                return true;
            }
            // deletes the retrieved element, do not use for non integral types
            bool pop(T & v)
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                if (m_queue.empty())
                    return false;
                v = m_queue.front();
                m_queue.pop();
                return true;
            }
            bool empty()
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                return m_queue.empty();
            }

        private:
            std::queue<T> m_queue;
            std::mutex    m_mutex;
        };
    public:
        ThreadPool() { init(); }
        ThreadPool(size_t nThreads) : ma_n_idle(0) { init(); resize(nThreads); }

        // the destructor waits for all tasks in the queue to be finished
        ~ThreadPool()
        {
            ma_shutdown = true;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cond.notify_all();  // stop all waiting threads
            }
            for (size_t i = 0; i < m_threads.size(); ++i)
            {
                if (m_threads[i]->joinable())
                    m_threads[i]->join();
            }
        }

        // get the number of running threads in the pool
        inline size_t size() const { return m_threads.size(); }

        // number of idle threads
        inline size_t n_idle() const { return ma_n_idle; }

        // whether there are any threads
        inline bool active() const { return ma_n_idle != m_threads.size(); }

        // get a specific thread
        inline std::thread & get_thread(size_t i) { return *m_threads.at(i); }

        // change the number of threads in the pool
        // should be called from one thread, otherwise be careful to not interleave, also with interrupt()
        void resize(size_t nThreads)
        {
            if (!ma_interrupt && !ma_kill)
            {
                size_t oldNThreads = m_threads.size();

                // if the number of threads is increased
                if (oldNThreads <= nThreads)
                {
                    m_threads.resize(nThreads);
                    m_abort.resize(nThreads);

                    for (size_t i = oldNThreads; i < nThreads; ++i)
                    {
                        m_abort[i] = std::make_shared<std::atomic<bool>>(false);
                        setup_thread(i);
                    }
                }
                // the number of threads is decreased
                else
                {
                    for (size_t i = oldNThreads - 1; i >= nThreads; --i)
                    {
                        *m_abort[i] = true;  // this thread will finish
                        m_threads[i]->detach();
                    }

                    {
                        // stop the detached threads that were waiting
                        std::unique_lock<std::mutex> lock(m_mutex);
                        m_cond.notify_all();
                    }

                    m_threads.resize(nThreads); // safe to delete because the threads are detached
                    m_abort.resize(nThreads); // safe to delete because the threads have copies of shared_ptr of the flags, not originals
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
        auto push(F && f, Args&&... args)
            -> std::future<typename std::result_of<F(Args...)>::type>
        {
            using return_type = typename std::result_of<F(Args...)>::type;
            if (!ma_interrupt && !ma_kill)
            {
                auto pck = std::make_shared<std::packaged_task<return_type()>>(
                    std::bind(std::forward<F>(f), std::forward<Args>(args)...)
                );
                auto _f  = new std::function<void()>([pck](){ (*pck)(); });

                m_queue.push(_f);
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cond.notify_one();
                return pck->get_future();
            }
            else return std::future<return_type>();
        }

    private:
        // deleted
        ThreadPool(const ThreadPool &);// = delete;
        ThreadPool(ThreadPool &&);// = delete;
        ThreadPool & operator=(const ThreadPool &);// = delete;
        ThreadPool & operator=(ThreadPool &&);// = delete;

        // clear all tasks
        void clear_queue()
        {
            std::function<void()> *_f = nullptr;
            while (m_queue.pop(_f))
                delete _f; // empty the queue
        }

        // wait for all computing threads to finish
        // may be called asynchronously to not pause the calling thread while waiting
        // if kill == true, queued tasks are not completed and removed
        void interrupt(bool kill = false)
        {
            if (kill)
            {
                if (ma_kill) return;
                ma_kill = true;
                clear_queue();
            }
            else
            {
                if (ma_interrupt || ma_kill) return;
                ma_interrupt = true;  // give the waiting threads a command to finish
            }

            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cond.notify_all();  // stop all waiting threads
            }
            // wait for the computing threads to finish
            // if ma_shutdown is set the destructor will wait for the queue to finish
            std::unique_lock<std::mutex> lock(m_imutex);
            m_icond.wait(lock, [ this ]
            {
                return (ma_n_idle == m_threads.size() && m_queue.empty()) || ma_shutdown;
            });
        }

        // init all flags
        void init() { ma_kill = false; ma_interrupt = false; ma_shutdown = false; }

        // each thread pops jobs from the queue until:
        //  - its abort flag is set (used when resizing the pool)
        //  - a global interrupt (shutdown/interrupt) is set, then only idle threads terminate
        void setup_thread(size_t i)
        {
            // a copy of the shared ptr to the abort
            std::shared_ptr<std::atomic<bool>> abort_ptr(m_abort[i]);

            auto f = [this, abort_ptr]()
            {
                std::atomic<bool> & abort = *abort_ptr;
                std::function<void()> *_f = nullptr;
                bool more_tasks = m_queue.pop(_f);

                while (true)
                {
                    // if there is anything in the queue
                    while (more_tasks)
                    {
                        // at return, delete the function even if an exception occurred
                        std::unique_ptr<std::function<void()>> func(_f);
                        (*_f)();
                        if (abort)
                            return; // return even if the queue is not empty yet
                        else
                            more_tasks = m_queue.pop(_f);
                    }

                    std::unique_lock<std::mutex> lock(m_mutex);
                    ++ma_n_idle;
                    {
                        std::unique_lock<std::mutex> lock(m_imutex);
                        m_icond.notify_one();
                    }
                    // the queue is empty here, wait for the next command
                    m_cond.wait(lock, [ this, &_f, &more_tasks, &abort ]
                    {
                        more_tasks = m_queue.pop(_f);
                        return abort || ma_shutdown || more_tasks;
                    });
                    --ma_n_idle;
                    if (!more_tasks && (ma_shutdown || abort)) return;
                }
            };

            m_threads[i].reset(new std::thread(f));
        }

        std::vector<std::unique_ptr<std::thread>>       m_threads;
        std::vector<std::shared_ptr<std::atomic<bool>>> m_abort;
        Queue<std::function<void()>*>                   m_queue;

        std::atomic<bool>   ma_interrupt, ma_kill, ma_shutdown;
        std::atomic<size_t> ma_n_idle;

        std::mutex              m_mutex, m_imutex;
        std::condition_variable m_cond, m_icond;
    };
}

#endif /* _THREADPOOL_H_ */
