#pragma once

#include "pipeline/IEventRouter.h"
#include "source/IMarketDataSource.h"

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

namespace mdp
{
    class Producer
    {
    public:
        explicit Producer(IEventRouter& eventRouter);
        ~Producer();

        void start();
        void stop();

        std::size_t getProducedCount() const;
        std::size_t getRejectedCount() const;
        std::size_t getActiveProducerCount() const;

    private:
        void produceLoop(std::size_t producerIndex);
        static std::vector<std::unique_ptr<IMarketDataSource>> createSources();

    private:
        std::vector<std::unique_ptr<IMarketDataSource>> m_sources;
        IEventRouter& m_eventRouter;

        std::vector<std::thread> m_workerThreads;
        std::atomic<bool> m_running{ false };
        std::atomic<std::size_t> m_producedCount{ 0 };
        std::atomic<std::size_t> m_rejectedCount{ 0 };
    };
}
