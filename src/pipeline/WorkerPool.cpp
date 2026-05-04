#include "pipeline/WorkerPool.h"

#include "config/AppConfig.h"
#include "pipeline/BlockingBoundedEventQueue.h"
#include "pipeline/LockFreeRingEventQueue.h"
#include "processing/SymbolStats.h"
#include "util/Clock.h"

#include <functional>
#include <limits>
#include <stdexcept>

namespace mdp::pipeline
{
    using metrics::LatencyRecorder;
    using processing::EventProcessor;
    using processing::SymbolState;
    using processing::SymbolStatistics;
    using processing::SymbolStats;

    namespace
    {
        std::unique_ptr<IEventQueue> createEventQueue()
        {
            const auto& workerConfig = config::get().worker();

            switch (workerConfig.workerQueueType)
            {
            case config::QueueType::BlockingBounded:
                return std::make_unique<BlockingBoundedEventQueue>(
                    workerConfig.workerQueueCapacity,
                    workerConfig.workerQueueFullStrategy);

            case config::QueueType::LockFreeRing:
                return std::make_unique<LockFreeRingEventQueue>(
                    workerConfig.workerQueueCapacity,
                    workerConfig.workerQueueFullStrategy);
            }

            throw std::runtime_error("Unsupported worker queue type");
        }

        std::uint64_t getAverageLatencyAcross(
            const std::vector<std::unique_ptr<WorkerPool::PartitionContext>>& partitions,
            const LatencyRecorder& (EventProcessor::* getRecorder)() const)
        {
            std::uint64_t totalWeightedLatency = 0;
            std::uint64_t totalCount = 0;

            for (const auto& partition : partitions)
            {
                const LatencyRecorder& latency = (partition->processor.*getRecorder)();
                const std::uint64_t count = latency.getCount();

                if (count == 0)
                {
                    continue;
                }

                totalWeightedLatency += latency.getAverageLatencyNs() * count;
                totalCount += count;
            }

            if (totalCount == 0)
            {
                return 0;
            }

            return totalWeightedLatency / totalCount;
        }

        std::uint64_t getMinLatencyAcross(
            const std::vector<std::unique_ptr<WorkerPool::PartitionContext>>& partitions,
            const LatencyRecorder& (EventProcessor::* getRecorder)() const)
        {
            std::uint64_t globalMin = std::numeric_limits<std::uint64_t>::max();
            bool foundActivePartition = false;

            for (const auto& partition : partitions)
            {
                const LatencyRecorder& latency = (partition->processor.*getRecorder)();

                if (latency.getCount() == 0)
                {
                    continue;
                }

                const std::uint64_t partitionMin = latency.getMinLatencyNs();
                if (partitionMin < globalMin)
                {
                    globalMin = partitionMin;
                }

                foundActivePartition = true;
            }

            return foundActivePartition ? globalMin : 0;
        }

        std::uint64_t getMaxLatencyAcross(
            const std::vector<std::unique_ptr<WorkerPool::PartitionContext>>& partitions,
            const LatencyRecorder& (EventProcessor::* getRecorder)() const)
        {
            std::uint64_t globalMax = 0;
            bool foundActivePartition = false;

            for (const auto& partition : partitions)
            {
                const LatencyRecorder& latency = (partition->processor.*getRecorder)();

                if (latency.getCount() == 0)
                {
                    continue;
                }

                const std::uint64_t partitionMax = latency.getMaxLatencyNs();
                if (partitionMax > globalMax)
                {
                    globalMax = partitionMax;
                }

                foundActivePartition = true;
            }

            return foundActivePartition ? globalMax : 0;
        }

        LatencyRecorder::BucketSnapshot getLatencyBucketsAcross(
            const std::vector<std::unique_ptr<WorkerPool::PartitionContext>>& partitions,
            const LatencyRecorder& (EventProcessor::* getRecorder)() const)
        {
            LatencyRecorder::BucketSnapshot mergedBuckets{};

            for (const auto& partition : partitions)
            {
                const LatencyRecorder& latency = (partition->processor.*getRecorder)();
                const auto partitionBuckets = latency.getBucketSnapshot();

                for (std::size_t i = 0; i < mergedBuckets.size(); ++i)
                {
                    mergedBuckets[i] += partitionBuckets[i];
                }
            }

            return mergedBuckets;
        }

        std::uint64_t getLatencyCountAcross(
            const std::vector<std::unique_ptr<WorkerPool::PartitionContext>>& partitions,
            const LatencyRecorder& (EventProcessor::* getRecorder)() const)
        {
            std::uint64_t totalCount = 0;

            for (const auto& partition : partitions)
            {
                const LatencyRecorder& latency = (partition->processor.*getRecorder)();
                totalCount += latency.getCount();
            }

            return totalCount;
        }
    }

    WorkerPool::PartitionContext::PartitionContext(mdp::output::IEventSink* eventSink)
        : queue(createEventQueue()),
          processor(eventSink)
    {
    }

    WorkerPool::WorkerPool(std::size_t workerCount)
        : WorkerPool(workerCount, nullptr)
    {
    }

    WorkerPool::WorkerPool(std::size_t workerCount, mdp::output::IEventSink* eventSink)
    {
        if (workerCount == 0)
        {
            throw std::invalid_argument("WorkerPool requires at least one worker");
        }

        m_partitions.reserve(workerCount);

        for (std::size_t i = 0; i < workerCount; ++i)
        {
            m_partitions.push_back(std::make_unique<PartitionContext>(eventSink));
        }
    }

    WorkerPool::~WorkerPool()
    {
        stop();
        join();
    }

    void WorkerPool::start()
    {
        if (m_running.exchange(true))
        {
            return;
        }

        m_workers.reserve(m_partitions.size());

        for (std::size_t i = 0; i < m_partitions.size(); ++i)
        {
            m_workers.emplace_back(&WorkerPool::workerLoop, this, i);
        }
    }

    void WorkerPool::stop(bool drainQueuedEvents)
    {
        if (!m_running.exchange(false))
        {
            return;
        }

        for (auto& partition : m_partitions)
        {
            if (drainQueuedEvents)
            {
                partition->queue->close();
            }
            else
            {
                partition->queue->closeAndDiscard();
            }
        }
    }

    bool WorkerPool::submit(const MarketDataEvent& event)
    {
        const std::size_t index = getPartitionIndex(event.symbol);
        auto& partition = *m_partitions[index];

        MarketDataEvent queuedEvent = event;
        queuedEvent.enqueueTimestampNs = clock::nowNs();

        const bool pushed = partition.queue->push(queuedEvent);
        if (pushed)
        {
            partition.acceptedCount.fetch_add(1);
        }

        return pushed;
    }

    void WorkerPool::join()
    {
        for (auto& worker : m_workers)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }

        m_workers.clear();
    }

    void WorkerPool::workerLoop(std::size_t partitionIndex)
    {
        auto& partition = *m_partitions[partitionIndex];

        MarketDataEvent event;

        while (partition.queue->pop(event))
        {
            partition.processor.process(event);
        }
    }

    std::size_t WorkerPool::getPartitionIndex(const Symbol& symbol) const
    {
        return std::hash<Symbol>{}(symbol) % m_partitions.size();
    }

    std::size_t WorkerPool::getWorkerCount() const
    {
        return m_partitions.size();
    }

    std::uint64_t WorkerPool::getProcessedCount() const
    {
        std::uint64_t total = 0;

        for (const auto& p : m_partitions)
        {
            total += p->processor.getMetrics().getProcessed();
        }

        return total;
    }

    std::uint64_t WorkerPool::getAcceptedCount() const
    {
        std::uint64_t total = 0;

        for (const auto& p : m_partitions)
        {
            total += p->acceptedCount.load();
        }

        return total;
    }

    std::vector<WorkerPool::PartitionMetrics> WorkerPool::getPartitionMetrics() const
    {
        std::vector<PartitionMetrics> metrics;
        metrics.reserve(m_partitions.size());

        for (std::size_t i = 0; i < m_partitions.size(); ++i)
        {
            const auto& partition = *m_partitions[i];
            const QueueMetricsSnapshot queueMetrics = partition.queue->getMetricsSnapshot();

            PartitionMetrics snapshot;
            snapshot.partitionIndex = i;
            snapshot.currentDepth = queueMetrics.currentDepth;
            snapshot.maxDepth = queueMetrics.maxDepth;
            snapshot.droppedCount = queueMetrics.droppedCount;
            snapshot.failedEnqueueCount = queueMetrics.failedEnqueueCount;
            snapshot.acceptedCount = partition.acceptedCount.load();
            snapshot.processedCount = partition.processor.getMetrics().getProcessed();
            snapshot.capacity = partition.queue->capacity();

            metrics.push_back(snapshot);
        }

        return metrics;
    }

    std::unordered_map<Symbol, SymbolState> WorkerPool::getStateSnapshot() const
    {
        std::unordered_map<Symbol, SymbolState> merged;

        for (const auto& partition : m_partitions)
        {
            const auto snapshot = partition->processor.getStateSnapshot();
            merged.insert(snapshot.begin(), snapshot.end());
        }

        return merged;
    }

    std::unordered_map<Symbol, SymbolStatistics> WorkerPool::getStatsSnapshot() const
    {
        std::unordered_map<Symbol, SymbolStatistics> merged;

        for (const auto& partition : m_partitions)
        {
            const auto snapshot = partition->processor.getStatsSnapshot();

            for (const auto& [symbol, stats] : snapshot)
            {
                SymbolStats::mergeInto(merged, symbol, stats);
            }
        }

        return merged;
    }

    std::size_t WorkerPool::getTrackedStateSymbolCount() const
    {
        std::size_t total = 0;

        for (const auto& partition : m_partitions)
        {
            total += partition->processor.getTrackedStateSymbolCount();
        }

        return total;
    }

    std::size_t WorkerPool::getTrackedStatsSymbolCount() const
    {
        std::size_t total = 0;

        for (const auto& partition : m_partitions)
        {
            total += partition->processor.getTrackedStatsSymbolCount();
        }

        return total;
    }

    std::uint64_t WorkerPool::getAverageLatencyNs() const
    {
        std::uint64_t totalWeightedLatency = 0;
        std::uint64_t totalCount = 0;

        for (const auto& partition : m_partitions)
        {
            const LatencyRecorder& latency = partition->processor.getLatency();
            const std::uint64_t count = latency.getCount();

            if (count == 0)
            {
                continue;
            }

            totalWeightedLatency += latency.getAverageLatencyNs() * count;
            totalCount += count;
        }

        if (totalCount == 0)
        {
            return 0;
        }

        return totalWeightedLatency / totalCount;
    }

    std::uint64_t WorkerPool::getMinLatencyNs() const
    {
        std::uint64_t globalMin = std::numeric_limits<std::uint64_t>::max();
        bool foundActivePartition = false;

        for (const auto& partition : m_partitions)
        {
            const LatencyRecorder& latency = partition->processor.getLatency();

            if (latency.getCount() == 0)
            {
                continue;
            }

            const std::uint64_t partitionMin = latency.getMinLatencyNs();
            if (partitionMin < globalMin)
            {
                globalMin = partitionMin;
            }

            foundActivePartition = true;
        }

        if (!foundActivePartition)
        {
            return 0;
        }

        return globalMin;
    }

    std::uint64_t WorkerPool::getMaxLatencyNs() const
    {
        std::uint64_t globalMax = 0;
        bool foundActivePartition = false;

        for (const auto& partition : m_partitions)
        {
            const LatencyRecorder& latency = partition->processor.getLatency();

            if (latency.getCount() == 0)
            {
                continue;
            }

            const std::uint64_t partitionMax = latency.getMaxLatencyNs();
            if (partitionMax > globalMax)
            {
                globalMax = partitionMax;
            }

            foundActivePartition = true;
        }

        if (!foundActivePartition)
        {
            return 0;
        }

        return globalMax;
    }

    std::uint64_t WorkerPool::getPercentileLatencyNs(double percentile) const
    {
        const LatencyRecorder::BucketSnapshot mergedBuckets = getLatencyBucketSnapshot();
        std::uint64_t totalCount = 0;

        for (const auto& partition : m_partitions)
        {
            const LatencyRecorder& latency = partition->processor.getLatency();
            totalCount += latency.getCount();
        }

        return LatencyRecorder::percentileFromBuckets(
            mergedBuckets,
            totalCount,
            percentile);
    }

    LatencyRecorder::BucketSnapshot WorkerPool::getLatencyBucketSnapshot() const
    {
        LatencyRecorder::BucketSnapshot mergedBuckets{};

        for (const auto& partition : m_partitions)
        {
            const LatencyRecorder& latency = partition->processor.getLatency();
            const auto partitionBuckets = latency.getBucketSnapshot();

            for (std::size_t i = 0; i < mergedBuckets.size(); ++i)
            {
                mergedBuckets[i] += partitionBuckets[i];
            }
        }

        return mergedBuckets;
    }

    std::uint64_t WorkerPool::getAverageQueueWaitLatencyNs() const
    {
        return getAverageLatencyAcross(m_partitions, &EventProcessor::getQueueWaitLatency);
    }

    std::uint64_t WorkerPool::getMinQueueWaitLatencyNs() const
    {
        return getMinLatencyAcross(m_partitions, &EventProcessor::getQueueWaitLatency);
    }

    std::uint64_t WorkerPool::getMaxQueueWaitLatencyNs() const
    {
        return getMaxLatencyAcross(m_partitions, &EventProcessor::getQueueWaitLatency);
    }

    std::uint64_t WorkerPool::getPercentileQueueWaitLatencyNs(double percentile) const
    {
        const LatencyRecorder::BucketSnapshot mergedBuckets =
            getQueueWaitLatencyBucketSnapshot();
        const std::uint64_t totalCount =
            getLatencyCountAcross(m_partitions, &EventProcessor::getQueueWaitLatency);

        return LatencyRecorder::percentileFromBuckets(
            mergedBuckets,
            totalCount,
            percentile);
    }

    LatencyRecorder::BucketSnapshot WorkerPool::getQueueWaitLatencyBucketSnapshot() const
    {
        return getLatencyBucketsAcross(m_partitions, &EventProcessor::getQueueWaitLatency);
    }

    std::uint64_t WorkerPool::getValidCount() const
    {
        std::uint64_t total = 0;

        for (const auto& p : m_partitions)
        {
            total += p->processor.getMetrics().getValid();
        }

        return total;
    }

    std::uint64_t WorkerPool::getInvalidCount() const
    {
        std::uint64_t total = 0;

        for (const auto& p : m_partitions)
        {
            total += p->processor.getMetrics().getInvalid();
        }

        return total;
    }

    std::uint64_t WorkerPool::getDuplicateCount() const
    {
        std::uint64_t total = 0;

        for (const auto& p : m_partitions)
        {
            total += p->processor.getMetrics().getDuplicate();
        }

        return total;
    }

    std::uint64_t WorkerPool::getOutOfOrderCount() const
    {
        std::uint64_t total = 0;

        for (const auto& p : m_partitions)
        {
            total += p->processor.getMetrics().getOutOfOrder();
        }

        return total;
    }

    std::uint64_t WorkerPool::getSequenceGapCount() const
    {
        std::uint64_t total = 0;

        for (const auto& p : m_partitions)
        {
            total += p->processor.getMetrics().getSequenceGap();
        }

        return total;
    }
}
