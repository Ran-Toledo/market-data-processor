#pragma once

#include "core/MarketDataEvent.h"
#include "processing/EventProcessor.h"
#include "processing/SymbolStateStore.h"
#include "processing/SymbolStats.h"
#include "containers/queue/BoundedConcurrentQueue.h"

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
        struct PartitionMetrics
        {
            std::size_t partitionIndex{ 0 };
            std::size_t currentDepth{ 0 };
            std::size_t maxDepth{ 0 };
            std::uint64_t droppedCount{ 0 };
            std::uint64_t failedEnqueueCount{ 0 };
            std::uint64_t acceptedCount{ 0 };
            std::uint64_t processedCount{ 0 };
        };

    public:
        WorkerPool(
            std::size_t workerCount,
            SymbolStateStore& stateStore,
            SymbolStats& symbolStats);

        ~WorkerPool();

        void start();
        void stop();
        bool submit(const MarketDataEvent& event);
        void join();

        std::size_t getWorkerCount() const;

        std::uint64_t getProcessedCount() const;
        std::uint64_t getAcceptedCount() const;
        std::uint64_t getAverageLatencyNs() const;
        std::uint64_t getMinLatencyNs() const;
        std::uint64_t getMaxLatencyNs() const;
        std::uint64_t getValidCount() const;
        std::uint64_t getInvalidCount() const;
        std::uint64_t getDuplicateCount() const;
        std::uint64_t getOutOfOrderCount() const;

        std::vector<PartitionMetrics> getPartitionMetrics() const;

    private:
        void workerLoop(std::size_t partitionIndex);
        std::size_t getPartitionIndex(const Symbol& symbol) const;

    private:
        struct PartitionContext
        {
            BoundedConcurrentQueue<MarketDataEvent> queue;
            EventProcessor processor;
            std::atomic<std::uint64_t> acceptedCount{ 0 };

            PartitionContext(SymbolStateStore& stateStore, SymbolStats& symbolStats)
                : queue(config::workerQueueCapacity, config::workerQueueFullStrategy)
                , processor(stateStore, symbolStats)
            {
            }
        };

        std::vector<std::unique_ptr<PartitionContext>> m_partitions;
        std::vector<std::thread> m_workers;
        std::atomic<bool> m_running{ false };
    };
}
