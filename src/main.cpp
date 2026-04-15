// main.cpp
#include "config/AppConfig.h"
#include "pipeline/Producer.h"
#include "pipeline/WorkerPool.h"
#include "processing/SymbolStats.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

namespace
{
    struct ThroughputSample
    {
        std::size_t producedCount{ 0 };
        std::size_t rejectedCount{ 0 };
        std::uint64_t processedCount{ 0 };
    };

    std::filesystem::path getConfigPath()
    {
        return std::filesystem::path(__FILE__).parent_path().parent_path()
            / "market-data-processor.ini";
    }

    double perSecond(std::uint64_t count, double elapsedSeconds)
    {
        if (elapsedSeconds <= 0.0)
        {
            return 0.0;
        }

        return static_cast<double>(count) / elapsedSeconds;
    }

    bool isNearCapacity(const mdp::pipeline::WorkerPool::PartitionMetrics& metrics)
    {
        return metrics.capacity > 0 &&
            (metrics.currentDepth * 10 >= metrics.capacity * 9);
    }

    bool reachedNearCapacity(const mdp::pipeline::WorkerPool::PartitionMetrics& metrics)
    {
        return metrics.capacity > 0 &&
            (metrics.maxDepth * 10 >= metrics.capacity * 9);
    }

    void printProducerMode(const mdp::pipeline::Producer& producer)
    {
        if (mdp::config::get().producer().producerCount != producer.getActiveProducerCount())
        {
            std::cout << "Configured producerCount="
                << mdp::config::get().producer().producerCount
                << ", using " << producer.getActiveProducerCount()
                << " producer thread with burst generation." << std::endl;
        }
    }

    void printPeriodicSummary(
        const mdp::pipeline::Producer& producer,
        const mdp::pipeline::WorkerPool& workerPool,
        ThroughputSample& previousSample,
        const std::chrono::steady_clock::time_point& previousTime,
        const std::chrono::steady_clock::time_point& currentTime)
    {
        const auto partitionMetrics = workerPool.getPartitionMetrics();
        const std::size_t producedCount = producer.getProducedCount();
        const std::size_t rejectedCount = producer.getRejectedCount();
        const std::uint64_t processedCount = workerPool.getProcessedCount();

        const double elapsedSeconds =
            std::chrono::duration_cast<std::chrono::duration<double>>(
                currentTime - previousTime).count();

        const std::uint64_t producedDelta = producedCount - previousSample.producedCount;
        const std::uint64_t rejectedDelta = rejectedCount - previousSample.rejectedCount;
        const std::uint64_t processedDelta = processedCount - previousSample.processedCount;

        std::size_t totalDepth = 0;
        std::size_t totalCapacity = 0;
        std::size_t maxDepthSeen = 0;
        std::size_t nearCapacityQueues = 0;
        std::size_t saturatedQueues = 0;

        for (const auto& metrics : partitionMetrics)
        {
            totalDepth += metrics.currentDepth;
            totalCapacity += metrics.capacity;
            maxDepthSeen = std::max(maxDepthSeen, metrics.maxDepth);

            if (isNearCapacity(metrics))
            {
                ++nearCapacityQueues;
            }

            if (reachedNearCapacity(metrics))
            {
                ++saturatedQueues;
            }
        }

        std::cout << "[summary] produced/sec=" << perSecond(producedDelta, elapsedSeconds)
            << " processed/sec=" << perSecond(processedDelta, elapsedSeconds)
            << " rejected/sec=" << perSecond(rejectedDelta, elapsedSeconds)
            << " queueDepth=" << totalDepth << '/' << totalCapacity
            << " maxDepthSeen=" << maxDepthSeen
            << " nearCapacityQueues=" << nearCapacityQueues << '/' << partitionMetrics.size()
            << " saturatedQueues=" << saturatedQueues << '/' << partitionMetrics.size()
            << std::endl;

        previousSample.producedCount = producedCount;
        previousSample.rejectedCount = rejectedCount;
        previousSample.processedCount = processedCount;
    }

    void runForConfiguredDuration(mdp::pipeline::Producer& producer, const mdp::pipeline::WorkerPool& workerPool)
    {
        const auto startTime = std::chrono::steady_clock::now();
        const auto endTime =
            startTime + std::chrono::seconds(
                mdp::config::get().runtime().appRuntimeSeconds);

        ThroughputSample previousSample;
        auto previousSummaryTime = startTime;

        while (std::chrono::steady_clock::now() < endTime)
        {
            const auto now = std::chrono::steady_clock::now();
            const auto remaining = endTime - now;
            const auto sleepFor = std::min(
                std::chrono::milliseconds(
                    mdp::config::get().runtime().periodicSummaryIntervalMs),
                std::chrono::duration_cast<std::chrono::milliseconds>(remaining));

            if (sleepFor.count() > 0)
            {
                std::this_thread::sleep_for(sleepFor);
            }

            const auto summaryTime = std::chrono::steady_clock::now();
            printPeriodicSummary(
                producer,
                workerPool,
                previousSample,
                previousSummaryTime,
                summaryTime);
            previousSummaryTime = summaryTime;
        }
    }

    void printProcessingSummary(
        const mdp::pipeline::Producer& producer,
        const mdp::pipeline::WorkerPool& workerPool,
        double elapsedSeconds)
    {
        if (!mdp::config::get().reporting().printProcessingStatsSummary)
        {
            return;
        }

        const std::size_t producedCount = producer.getProducedCount();
        const std::size_t rejectedCount = producer.getRejectedCount();
        const std::size_t generatedCount = producedCount + rejectedCount;
        const std::uint64_t processedCount = workerPool.getProcessedCount();

        std::cout << "Generated count: " << generatedCount << std::endl;
        std::cout << "Accepted count: " << producedCount << std::endl;
        std::cout << "Rejected count: " << rejectedCount << std::endl;
        std::cout << "Processed count: " << processedCount << std::endl;
        std::cout << "Valid events: " << workerPool.getValidCount() << std::endl;
        std::cout << "Invalid events: " << workerPool.getInvalidCount() << std::endl;
        std::cout << "Duplicate events: " << workerPool.getDuplicateCount() << std::endl;
        std::cout << "Out-of-order events: " << workerPool.getOutOfOrderCount() << std::endl;
        std::cout << "Sequence gaps: " << workerPool.getSequenceGapCount() << std::endl;
        std::cout << "Tracked symbols in worker-local state: "
            << workerPool.getTrackedStateSymbolCount() << std::endl;
        std::cout << "Tracked symbols in worker-local stats: "
            << workerPool.getTrackedStatsSymbolCount() << std::endl;
        std::cout << "Produced throughput: "
            << perSecond(producedCount, elapsedSeconds) << " events/sec" << std::endl;
        std::cout << "Processed throughput: "
            << perSecond(processedCount, elapsedSeconds) << " events/sec" << std::endl;
        std::cout << "Average latency: "
            << workerPool.getAverageLatencyNs() << " ns" << std::endl;
        std::cout << "Min latency: "
            << workerPool.getMinLatencyNs() << " ns" << std::endl;
        std::cout << "Max latency: "
            << workerPool.getMaxLatencyNs() << " ns" << std::endl;
        std::cout << "P50 latency: "
            << workerPool.getPercentileLatencyNs(50.0) << " ns" << std::endl;
        std::cout << "P95 latency: "
            << workerPool.getPercentileLatencyNs(95.0) << " ns" << std::endl;
        std::cout << "P99 latency: "
            << workerPool.getPercentileLatencyNs(99.0) << " ns" << std::endl;
        std::cout << "Average queue wait: "
            << workerPool.getAverageQueueWaitLatencyNs() << " ns" << std::endl;
        std::cout << "P99 queue wait: "
            << workerPool.getPercentileQueueWaitLatencyNs(99.0) << " ns" << std::endl;
    }

    void printQueueSummary(const mdp::pipeline::WorkerPool& workerPool)
    {
        if (!mdp::config::get().reporting().printQueueMetricsSummary)
        {
            return;
        }

        const auto queueMetrics = workerPool.getPartitionMetrics();

        std::cout << "\nPer-queue metrics:\n";

        for (const auto& metrics : queueMetrics)
        {
            std::cout << "Queue " << metrics.partitionIndex << '\n';
            std::cout << "  Current depth: " << metrics.currentDepth
                << "/" << metrics.capacity << '\n';
            std::cout << "  Max depth: " << metrics.maxDepth << '\n';
            std::cout << "  Drop count: " << metrics.droppedCount << '\n';
            std::cout << "  Enqueue failures: " << metrics.failedEnqueueCount << '\n';
            std::cout << "  Near capacity: "
                << (reachedNearCapacity(metrics) ? "yes" : "no") << '\n';
        }
    }

    void printSymbolSummary(const mdp::pipeline::WorkerPool& workerPool)
    {
        if (!mdp::config::get().reporting().printSymbolStatsSummary)
        {
            return;
        }

        const auto stateSnapshot = workerPool.getStateSnapshot();
        const auto statsSnapshot = workerPool.getStatsSnapshot();

        std::cout << "\nSymbol summary:\n";

        for (const auto& [symbol, state] : stateSnapshot)
        {
            std::cout << "Symbol: " << symbol << '\n';
            std::cout << "  Last price: " << state.lastPrice << '\n';
            std::cout << "  Last volume: " << state.lastVolume << '\n';
            std::cout << "  Last sequence: " << state.lastSequenceNumber << '\n';

            const auto statsIt = statsSnapshot.find(symbol);
            if (statsIt != statsSnapshot.end())
            {
                const mdp::processing::SymbolStatistics& stats = statsIt->second;

                std::cout << "  Event count: " << stats.eventCount << '\n';
                std::cout << "  Total volume: " << stats.totalVolume << '\n';
                std::cout << "  Min price: " << stats.minPrice << '\n';
                std::cout << "  Max price: " << stats.maxPrice << '\n';
                std::cout << "  Avg price: " << stats.averagePrice << '\n';
            }

            std::cout << '\n';
        }
    }
}

int main()
{
    mdp::config::loadFromFile(getConfigPath());

    mdp::pipeline::WorkerPool workerPool(mdp::config::get().runtime().numWorkers);
    mdp::pipeline::Producer producer(workerPool);

    printProducerMode(producer);
    std::cout << "Starting pipeline..." << std::endl;

    workerPool.start();
    producer.start();

    const auto startTime = std::chrono::steady_clock::now();
    runForConfiguredDuration(producer, workerPool);

    std::cout << "Stopping pipeline..." << std::endl;

    producer.requestStop();
    workerPool.stop();
    producer.join();
    workerPool.join();

    const auto endTime = std::chrono::steady_clock::now();
    const auto elapsedMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime).count();
    const double elapsedSeconds = static_cast<double>(elapsedMs) / 1000.0;

    std::cout << "Elapsed time: " << elapsedSeconds << " seconds" << std::endl;

    printProcessingSummary(producer, workerPool, elapsedSeconds);
    printQueueSummary(workerPool);
    printSymbolSummary(workerPool);

    return 0;
}
