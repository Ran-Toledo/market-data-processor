#pragma once

namespace mdp
{
    template<typename T>
    void ThreadSafeQueue<T>::push(const T& item)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (m_isClosed)
            {
                return;
            }

            m_queue.push(item);
        }

        m_cv.notify_one();
    }

    template<typename T>
    bool ThreadSafeQueue<T>::pop(T& item)
    {
        std::unique_lock<std::mutex> lock(m_mutex);

        m_cv.wait(lock, [this]()
            {
                return m_isClosed || !m_queue.empty();
            });

        if (m_queue.empty())
        {
            return false;
        }

        item = m_queue.front();
        m_queue.pop();
        return true;
    }

    template<typename T>
    bool ThreadSafeQueue<T>::tryPop(T& item)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_queue.empty())
        {
            return false;
        }

        item = m_queue.front();
        m_queue.pop();
        return true;
    }

    template<typename T>
    void ThreadSafeQueue<T>::close()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_isClosed = true;
        }

        m_cv.notify_all();
    }

    template<typename T>
    bool ThreadSafeQueue<T>::empty() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }
}
