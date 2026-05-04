#pragma once

#include "config/QueueTypes.h"
#include "pipeline/IEventQueue.h"

#include <atomic>
#include <cstddef>
#include <memory>
#include <thread>
#include <vector>

namespace mdp::pipeline
{
    class LockFreeRingEventQueue final : public IEventQueue
    {
    public:
        LockFreeRingEventQueue(
            std::size_t capacity,
            config::QueueFullPolicy fullPolicy);

        bool push(const MarketDataEvent& event) override;
        bool pop(MarketDataEvent& event) override;
        void close() override;
        void closeAndDiscard() override;
        std::size_t capacity() const override;
        QueueMetricsSnapshot getMetricsSnapshot() const override;

    private:
        struct Cell
        {
            std::atomic<std::size_t> sequence{ 0 };
            MarketDataEvent event;
        };

        bool tryPush(const MarketDataEvent& event);
        bool tryPop(MarketDataEvent& event);
        void updateMaxDepth(std::size_t depth);

    private:
        std::size_t m_capacity{ 0 };
        config::QueueFullPolicy m_fullPolicy;
        std::unique_ptr<Cell[]> m_buffer;
        std::atomic<std::size_t> m_enqueuePos{ 0 };
        std::atomic<std::size_t> m_dequeuePos{ 0 };
        std::atomic<bool> m_closed{ false };
        std::atomic<bool> m_discardOnClose{ false };

        std::atomic<std::size_t> m_currentDepth{ 0 };
        std::atomic<std::size_t> m_maxDepth{ 0 };
        std::atomic<std::uint64_t> m_droppedCount{ 0 };
        std::atomic<std::uint64_t> m_failedEnqueueCount{ 0 };
    };
}
