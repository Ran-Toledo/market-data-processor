#pragma once

#include "api/domain/MarketDataEvent.h"
#include "output/IEventSink.h"
#include "pipeline/IEventRouter.h"
#include "pipeline/IEventQueue.h"
#include "processing/EventProcessor.h"
#include "processing/SymbolStateStore.h"
#include "processing/SymbolStats.h"

#include <atomic>
#include <cstddef>
#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>

namespace mdp::pipeline
{
    class WorkerPool : public IEventRouter
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
            std::size_t capacity{ 0 };
        };

    public:
        explicit WorkerPool(std::size_t workerCount);
        WorkerPool(std::size_t workerCount, mdp::output::IEventSink* eventSink);

        ~WorkerPool();

        void start();
        void stop(bool drainQueuedEvents = true);
        bool submit(const MarketDataEvent& event) override;
        void join();

        std::size_t getWorkerCount() const;

        std::uint64_t getProcessedCount() const;
        std::uint64_t getAcceptedCount() const;
        std::uint64_t getAverageLatencyNs() const;
        std::uint64_t getMinLatencyNs() const;
        std::uint64_t getMaxLatencyNs() const;
        std::uint64_t getPercentileLatencyNs(double percentile) const;
        metrics::LatencyRecorder::BucketSnapshot getLatencyBucketSnapshot() const;
        std::uint64_t getAverageQueueWaitLatencyNs() const;
        std::uint64_t getMinQueueWaitLatencyNs() const;
        std::uint64_t getMaxQueueWaitLatencyNs() const;
        std::uint64_t getPercentileQueueWaitLatencyNs(double percentile) const;
        metrics::LatencyRecorder::BucketSnapshot getQueueWaitLatencyBucketSnapshot() const;
        std::uint64_t getValidCount() const;
        std::uint64_t getInvalidCount() const;
        std::uint64_t getDuplicateCount() const;
        std::uint64_t getOutOfOrderCount() const;
        std::uint64_t getSequenceGapCount() const;

        std::vector<PartitionMetrics> getPartitionMetrics() const;
        std::unordered_map<Symbol, processing::SymbolState> getStateSnapshot() const;
        std::unordered_map<Symbol, processing::SymbolStatistics> getStatsSnapshot() const;
        std::size_t getTrackedStateSymbolCount() const;
        std::size_t getTrackedStatsSymbolCount() const;

        struct PartitionContext
        {
            std::unique_ptr<IEventQueue> queue;
            processing::EventProcessor processor;
            std::atomic<std::uint64_t> acceptedCount{ 0 };

            explicit PartitionContext(mdp::output::IEventSink* eventSink);
        };

    private:
        void workerLoop(std::size_t partitionIndex);
        std::size_t getPartitionIndex(const Symbol& symbol) const;

    private:
        std::vector<std::unique_ptr<PartitionContext>> m_partitions;
        std::vector<std::thread> m_workers;
        std::atomic<bool> m_running{ false };
    };
}
