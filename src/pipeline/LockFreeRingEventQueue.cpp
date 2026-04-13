#include "pipeline/LockFreeRingEventQueue.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace mdp::pipeline
{
    LockFreeRingEventQueue::LockFreeRingEventQueue(
        std::size_t capacity,
        config::QueueFullPolicy fullPolicy)
        : m_capacity(capacity)
        , m_fullPolicy(fullPolicy)
        , m_buffer(std::make_unique<Cell[]>(capacity))
    {
        if (capacity == 0)
        {
            throw std::invalid_argument("LockFreeRingEventQueue capacity must be greater than zero");
        }

        for (std::size_t i = 0; i < capacity; ++i)
        {
            m_buffer[i].sequence.store(i, std::memory_order_relaxed);
        }
    }

    bool LockFreeRingEventQueue::push(const MarketDataEvent& event)
    {
        if (m_fullPolicy == config::QueueFullPolicy::DropIncoming)
        {
            if (tryPush(event))
            {
                return true;
            }

            m_droppedCount.fetch_add(1, std::memory_order_relaxed);
            m_failedEnqueueCount.fetch_add(1, std::memory_order_relaxed);
            return false;
        }

        while (!m_closed.load(std::memory_order_acquire))
        {
            if (tryPush(event))
            {
                return true;
            }

            std::this_thread::yield();
        }

        m_failedEnqueueCount.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    bool LockFreeRingEventQueue::pop(MarketDataEvent& event)
    {
        while (true)
        {
            if (m_discardOnClose.load(std::memory_order_acquire))
            {
                return false;
            }

            if (tryPop(event))
            {
                return true;
            }

            if (m_closed.load(std::memory_order_acquire) &&
                m_currentDepth.load(std::memory_order_acquire) == 0)
            {
                return false;
            }

            std::this_thread::yield();
        }
    }

    void LockFreeRingEventQueue::close()
    {
        m_closed.store(true, std::memory_order_release);
    }

    void LockFreeRingEventQueue::closeAndDiscard()
    {
        m_discardOnClose.store(true, std::memory_order_release);
        m_closed.store(true, std::memory_order_release);
        m_currentDepth.store(0, std::memory_order_release);
    }

    std::size_t LockFreeRingEventQueue::capacity() const
    {
        return m_capacity;
    }

    QueueMetricsSnapshot LockFreeRingEventQueue::getMetricsSnapshot() const
    {
        QueueMetricsSnapshot snapshot;
        snapshot.currentDepth = m_currentDepth.load(std::memory_order_relaxed);
        snapshot.maxDepth = m_maxDepth.load(std::memory_order_relaxed);
        snapshot.droppedCount = m_droppedCount.load(std::memory_order_relaxed);
        snapshot.failedEnqueueCount = m_failedEnqueueCount.load(std::memory_order_relaxed);
        return snapshot;
    }

    bool LockFreeRingEventQueue::tryPush(const MarketDataEvent& event)
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return false;
        }

        Cell* cell = nullptr;
        std::size_t pos = m_enqueuePos.load(std::memory_order_relaxed);

        while (true)
        {
            cell = &m_buffer[pos % m_capacity];
            const std::size_t seq = cell->sequence.load(std::memory_order_acquire);
            const auto diff = static_cast<std::intptr_t>(seq) - static_cast<std::intptr_t>(pos);

            if (diff == 0)
            {
                if (m_enqueuePos.compare_exchange_weak(
                    pos,
                    pos + 1,
                    std::memory_order_relaxed))
                {
                    break;
                }
            }
            else if (diff < 0)
            {
                return false;
            }
            else
            {
                pos = m_enqueuePos.load(std::memory_order_relaxed);
            }
        }

        cell->event = event;
        cell->sequence.store(pos + 1, std::memory_order_release);

        const std::size_t depth = m_currentDepth.fetch_add(1, std::memory_order_relaxed) + 1;
        updateMaxDepth(depth);
        return true;
    }

    bool LockFreeRingEventQueue::tryPop(MarketDataEvent& event)
    {
        Cell* cell = nullptr;
        std::size_t pos = m_dequeuePos.load(std::memory_order_relaxed);

        while (true)
        {
            cell = &m_buffer[pos % m_capacity];
            const std::size_t seq = cell->sequence.load(std::memory_order_acquire);
            const auto diff = static_cast<std::intptr_t>(seq) -
                static_cast<std::intptr_t>(pos + 1);

            if (diff == 0)
            {
                if (m_dequeuePos.compare_exchange_weak(
                    pos,
                    pos + 1,
                    std::memory_order_relaxed))
                {
                    break;
                }
            }
            else if (diff < 0)
            {
                return false;
            }
            else
            {
                pos = m_dequeuePos.load(std::memory_order_relaxed);
            }
        }

        event = cell->event;
        cell->sequence.store(pos + m_capacity, std::memory_order_release);
        m_currentDepth.fetch_sub(1, std::memory_order_relaxed);
        return true;
    }

    void LockFreeRingEventQueue::updateMaxDepth(std::size_t depth)
    {
        std::size_t prevMax = m_maxDepth.load(std::memory_order_relaxed);
        while (depth > prevMax &&
            !m_maxDepth.compare_exchange_weak(prevMax, depth, std::memory_order_relaxed))
        {
        }
    }
}
