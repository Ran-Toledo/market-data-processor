#pragma once

#include "core/MarketDataEvent.h"
#include "processing/EventProcessor.h"
#include "processing/SymbolStateStore.h"
#include "processing/SymbolStats.h"
#include "queue/ThreadSafeQueue.h"

#include <atomic>
#include <cstddef>
#include <memory>
#include <thread>
#include <vector>

namespace mdp
{
    class WorkerPool
    {
    public:
        WorkerPool(
            std::size_t workerCount,
            SymbolStateStore& stateStore,
            SymbolStats& symbolStats);

        ~WorkerPool();

        void start();
        void stop();
        void submit(const MarketDataEvent& event);
        void join();

        std::size_t getWorkerCount() const;

        std::uint64_t getProcessedCount() const;
        std::uint64_t getAverageLatencyNs() const;
        std::uint64_t getMinLatencyNs() const;
        std::uint64_t getMaxLatencyNs() const;

    private:
        void workerLoop(std::size_t partitionIndex);
        std::size_t getPartitionIndex(const Symbol& symbol) const;

    private:
        struct PartitionContext
        {
            ThreadSafeQueue<MarketDataEvent> queue;
            EventProcessor processor;

            PartitionContext(SymbolStateStore& stateStore, SymbolStats& symbolStats)
                : processor(stateStore, symbolStats)
            {
            }
        };

        std::vector<std::unique_ptr<PartitionContext>> m_partitions;
        std::vector<std::thread> m_workers;
        std::atomic<bool> m_running{ false };
    };
}
