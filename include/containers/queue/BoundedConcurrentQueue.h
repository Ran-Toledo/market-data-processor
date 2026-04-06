#pragma once

#include "core/AppConfig.h"

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <atomic>

namespace mdp
{
    struct QueueMetricsSnapshot
    {
        std::size_t currentDepth{ 0 };
        std::size_t maxDepth{ 0 };
        std::uint64_t droppedCount{ 0 };
        std::uint64_t failedEnqueueCount{ 0 };
    };

    template <typename T>
    class BoundedConcurrentQueue
    {
    public:
        explicit BoundedConcurrentQueue(
            std::size_t capacity,
            config::QueueFullPolicy fullPolicy = config::QueueFullPolicy::DropIncoming);

        bool push(const T& item);
        bool push(T&& item);
        bool pop(T& item);

        void close();
        bool isClosed() const;

        std::size_t size() const;
        std::size_t capacity() const;

        QueueMetricsSnapshot getMetricsSnapshot() const;

    private:
        template <typename U>
        bool pushImpl(U&& item);

    private:
        std::size_t m_capacity{ 0 };
        config::QueueFullPolicy m_fullPolicy;
        bool m_closed{ false };

        std::deque<T> m_queue;
        mutable std::mutex m_mutex;
        std::condition_variable m_notEmptyCondition;
        std::condition_variable m_notFullCondition;

        std::atomic<std::size_t> m_currentDepth{ 0 };
        std::atomic<std::size_t> m_maxDepth{ 0 };
        std::atomic<std::uint64_t> m_droppedCount{ 0 };
        std::atomic<std::uint64_t> m_failedEnqueueCount{ 0 };
    };
}

#include "containers/queue/BoundedConcurrentQueue.hpp"
