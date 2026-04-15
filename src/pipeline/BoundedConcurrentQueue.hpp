#pragma once

namespace mdp::pipeline
{
    template <typename T>
    BoundedConcurrentQueue<T>::BoundedConcurrentQueue(
        std::size_t capacity,
        config::QueueFullPolicy fullPolicy)
        : m_capacity(capacity)
        , m_fullPolicy(fullPolicy)
    {
    }

    template <typename T>
    bool BoundedConcurrentQueue<T>::push(const T& item)
    {
        return pushImpl(item);
    }

    template <typename T>
    bool BoundedConcurrentQueue<T>::push(T&& item)
    {
        return pushImpl(std::move(item));
    }

    template <typename T>
    bool BoundedConcurrentQueue<T>::pop(T& item)
    {
        std::unique_lock<std::mutex> lock(m_mutex);

        m_notEmptyCondition.wait(lock, [this]()
            {
                return m_closed || !m_queue.empty();
            });

        if (m_queue.empty())
        {
            return false;
        }

        item = std::move(m_queue.front());
        m_queue.pop_front();

        const std::size_t currentDepth = m_queue.size();
        m_currentDepth.store(currentDepth);

        lock.unlock();
        m_notFullCondition.notify_one();

        return true;
    }

    template <typename T>
    void BoundedConcurrentQueue<T>::close()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_closed = true;
        }

        m_notEmptyCondition.notify_all();
        m_notFullCondition.notify_all();
    }

    template <typename T>
    void BoundedConcurrentQueue<T>::closeAndDiscard()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_closed = true;
            m_queue.clear();
            m_currentDepth.store(0);
        }

        m_notEmptyCondition.notify_all();
        m_notFullCondition.notify_all();
    }

    template <typename T>
    bool BoundedConcurrentQueue<T>::isClosed() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_closed;
    }

    template <typename T>
    std::size_t BoundedConcurrentQueue<T>::size() const
    {
        return m_currentDepth.load();
    }

    template <typename T>
    std::size_t BoundedConcurrentQueue<T>::capacity() const
    {
        return m_capacity;
    }

    template <typename T>
    QueueMetricsSnapshot BoundedConcurrentQueue<T>::getMetricsSnapshot() const
    {
        QueueMetricsSnapshot snapshot;
        snapshot.currentDepth = m_currentDepth.load();
        snapshot.maxDepth = m_maxDepth.load();
        snapshot.droppedCount = m_droppedCount.load();
        snapshot.failedEnqueueCount = m_failedEnqueueCount.load();
        return snapshot;
    }

    template <typename T>
    template <typename U>
    bool BoundedConcurrentQueue<T>::pushImpl(U&& item)
    {
        std::unique_lock<std::mutex> lock(m_mutex);

        if (m_closed)
        {
            m_failedEnqueueCount.fetch_add(1);
            return false;
        }

        if (m_fullPolicy == config::QueueFullPolicy::BlockProducer)
        {
            m_notFullCondition.wait(lock, [this]()
                {
                    return m_closed || m_queue.size() < m_capacity;
                });

            if (m_closed)
            {
                m_failedEnqueueCount.fetch_add(1);
                return false;
            }
        }
        else
        {
            if (m_queue.size() >= m_capacity)
            {
                m_droppedCount.fetch_add(1);
                m_failedEnqueueCount.fetch_add(1);
                return false;
            }
        }

        m_queue.emplace_back(std::forward<U>(item));

        const std::size_t currentDepth = m_queue.size();
        m_currentDepth.store(currentDepth);

        std::size_t prevMax = m_maxDepth.load();
        while (currentDepth > prevMax &&
            !m_maxDepth.compare_exchange_weak(prevMax, currentDepth))
        {
        }

        lock.unlock();
        m_notEmptyCondition.notify_one();

        return true;
    }
}
