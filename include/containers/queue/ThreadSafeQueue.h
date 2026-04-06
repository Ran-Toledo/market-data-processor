#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>

namespace mdp
{
    template<typename T>
    class ThreadSafeQueue
    {
    public:
        ThreadSafeQueue() = default;

        void push(const T& item);
        bool pop(T& item);
        bool tryPop(T& item);
        void close();
        bool empty() const;

    private:
        std::queue<T> m_queue;
        mutable std::mutex m_mutex;
        std::condition_variable m_cv;
        bool m_isClosed = false;
    };
}

#include "containers/queue/ThreadSafeQueue.hpp"
