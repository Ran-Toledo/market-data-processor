// Producer.h
#pragma once

#include "core/MarketDataEvent.h"
#include "queue/ThreadSafeQueue.h"
#include "source/IMarketDataSource.h"

#include <atomic>
#include <cstddef>
#include <thread>

namespace mdp
{
    class Producer
    {
    public:
        Producer(
            IMarketDataSource& source,
            ThreadSafeQueue<MarketDataEvent>& queue);
        ~Producer();

        void start();
        void stop();

        std::size_t getProducedCount() const;

    private:
        void produceLoop();

    private:
        IMarketDataSource& m_source;
        ThreadSafeQueue<MarketDataEvent>& m_queue;
        std::thread m_workerThread;
        std::atomic<bool> m_running = false;
        std::atomic<std::size_t> m_producedCount = 0;
    };
}
