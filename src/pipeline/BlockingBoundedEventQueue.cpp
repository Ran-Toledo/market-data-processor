#include "pipeline/BlockingBoundedEventQueue.h"

namespace mdp
{
    BlockingBoundedEventQueue::BlockingBoundedEventQueue(
        std::size_t capacity,
        config::QueueFullPolicy fullPolicy)
        : m_queue(capacity, fullPolicy)
    {
    }

    bool BlockingBoundedEventQueue::push(const MarketDataEvent& event)
    {
        return m_queue.push(event);
    }

    bool BlockingBoundedEventQueue::pop(MarketDataEvent& event)
    {
        return m_queue.pop(event);
    }

    void BlockingBoundedEventQueue::close()
    {
        m_queue.close();
    }

    void BlockingBoundedEventQueue::closeAndDiscard()
    {
        m_queue.closeAndDiscard();
    }

    std::size_t BlockingBoundedEventQueue::capacity() const
    {
        return m_queue.capacity();
    }

    QueueMetricsSnapshot BlockingBoundedEventQueue::getMetricsSnapshot() const
    {
        return m_queue.getMetricsSnapshot();
    }
}
