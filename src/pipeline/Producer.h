#pragma once

#include "pipeline/IEventRouter.h"
#include "source/IMarketDataSource.h"

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

namespace mdp::pipeline
{
    struct ProducerOptions
    {
        std::size_t producerCount{ 1 };
        std::size_t producerBurstSize{ 1 };
        std::uint32_t producerSleepUs{ 1000 };
        bool enableEventLogging{ false };
        bool enableStatsLogging{ false };
        std::size_t statsLogInterval{ 1000 };
    };

    class Producer
    {
    public:
        explicit Producer(IEventRouter& eventRouter, ProducerOptions options = {});
        ~Producer();

        void start();
        void requestStop();
        void stop();
        void join();

        std::size_t getProducedCount() const;
        std::size_t getRejectedCount() const;
        std::size_t getActiveProducerCount() const;
        const ProducerOptions& getOptions() const { return m_options; }

    private:
        void produceLoop(std::size_t producerIndex);
        static std::vector<std::unique_ptr<source::IMarketDataSource>> createSources(
            std::size_t producerCount);

    private:
        std::vector<std::unique_ptr<source::IMarketDataSource>> m_sources;
        IEventRouter& m_eventRouter;
        ProducerOptions m_options;

        std::vector<std::thread> m_workerThreads;
        std::atomic<bool> m_running{ false };
        std::atomic<std::size_t> m_producedCount{ 0 };
        std::atomic<std::size_t> m_rejectedCount{ 0 };
    };
}
