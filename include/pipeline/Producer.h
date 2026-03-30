// Producer.h
#pragma once

#include "core/MarketDataEvent.h"
#include "queue/BoundedConcurrentQueue.h"
#include "source/IMarketDataSource.h"

#include <atomic>
#include <cstddef>

namespace mdp
{
    class Producer
    {
    public:
        Producer(
            IMarketDataSource& source,
            BoundedConcurrentQueue<MarketDataEvent>& queue);

        void run();

        std::size_t producedCount() const;

    private:
        IMarketDataSource& m_source;
        BoundedConcurrentQueue<MarketDataEvent>& m_queue;
        std::atomic<std::size_t> m_producedCount{ 0 };
    };
}
