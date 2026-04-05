#include "pipeline/WorkerPool.h"

#include <functional>
#include <limits>
#include <stdexcept>

namespace mdp
{
    WorkerPool::WorkerPool(
        std::size_t workerCount,
        SymbolStateStore& stateStore,
        SymbolStats& symbolStats)
    {
        if (workerCount == 0)
        {
            throw std::invalid_argument("WorkerPool requires at least one worker");
        }

        m_partitions.reserve(workerCount);

        for (std::size_t i = 0; i < workerCount; ++i)
        {
            m_partitions.push_back(
                std::make_unique<PartitionContext>(stateStore, symbolStats));
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

    void WorkerPool::stop()
    {
        if (!m_running.exchange(false))
        {
            return;
        }

        for (auto& partition : m_partitions)
        {
            partition->queue.close();
        }
    }

    void WorkerPool::submit(const MarketDataEvent& event)
    {
        const std::size_t index = getPartitionIndex(event.symbol);
        m_partitions[index]->queue.push(event);
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

        while (partition.queue.pop(event))
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
}
