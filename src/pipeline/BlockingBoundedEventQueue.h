#pragma once

#include "pipeline/IEventQueue.h"
#include "pipeline/BoundedConcurrentQueue.h"

namespace mdp
{
    class BlockingBoundedEventQueue final : public IEventQueue
    {
    public:
        BlockingBoundedEventQueue(
            std::size_t capacity,
            config::QueueFullPolicy fullPolicy);

        bool push(const MarketDataEvent& event) override;
        bool pop(MarketDataEvent& event) override;
        void close() override;
        void closeAndDiscard() override;
        std::size_t capacity() const override;
        QueueMetricsSnapshot getMetricsSnapshot() const override;

    private:
        BoundedConcurrentQueue<MarketDataEvent> m_queue;
    };
}
