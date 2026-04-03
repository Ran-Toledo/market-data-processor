// BoundedConcurrentQueue.h
#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>

namespace mdp
{
    template <typename T>
    class BoundedConcurrentQueue
    {
    public:
        explicit BoundedConcurrentQueue(std::size_t capacity);

        bool push(const T& item);
        bool push(T&& item);

        std::optional<T> pop();

        void close();

        bool isClosed() const;
        std::size_t size() const;
        std::size_t capacity() const;

    private:
        std::size_t m_capacity{ 0 };
        bool m_closed{ false };
        std::deque<T> m_queue;
        mutable std::mutex m_mutex;
        std::condition_variable m_notEmptyCondition;
        std::condition_variable m_notFullCondition;
    };
}
