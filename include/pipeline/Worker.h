// Worker.h
#pragma once

#include "core/MarketDataEvent.h"
#include "processing/EventProcessor.h"
#include "queue/BoundedConcurrentQueue.h"

#include <atomic>
#include <cstddef>

namespace mdp
{
    class Worker
    {
    public:
        Worker(
            BoundedConcurrentQueue<MarketDataEvent>& queue,
            EventProcessor& processor);

        void run();

        std::size_t processedCount() const;

    private:
        BoundedConcurrentQueue<MarketDataEvent>& m_queue;
        EventProcessor& m_processor;
        std::atomic<std::size_t> m_processedCount{ 0 };
    };
}
