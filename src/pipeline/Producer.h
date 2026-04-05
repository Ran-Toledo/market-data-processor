#pragma once

#include "core/MarketDataEvent.h"
#include "pipeline/WorkerPool.h"
#include "source/IMarketDataSource.h"

#include <atomic>
#include <thread>

namespace mdp
{
    class Producer
    {
    public:
        Producer(IMarketDataSource& source, WorkerPool& workerPool);
        ~Producer();

        void start();
        void stop();

        std::size_t getProducedCount() const;

    private:
        void produceLoop();

    private:
        IMarketDataSource& m_source;
        WorkerPool& m_workerPool;

        std::thread m_workerThread;
        std::atomic<bool> m_running{ false };
        std::atomic<std::size_t> m_producedCount{ 0 };
    };
}
