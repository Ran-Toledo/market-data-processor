// main.cpp
#include "config/AppConfig.h"
#include "network/TcpEventReceiver.h"
#include "pipeline/WorkerPool.h"
#include "processing/SymbolStats.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

namespace
{
    struct ThroughputSample
    {
        std::uint64_t receivedCount{ 0 };
        std::size_t rejectedCount{ 0 };
        std::uint64_t processedCount{ 0 };
    };

    struct ProcessorOptions
    {
        std::filesystem::path configPath;
    };

    std::filesystem::path getConfigPath()
    {
        return std::filesystem::path(__FILE__).parent_path().parent_path()
            / "market-data-processor.ini";
    }

    ProcessorOptions parseOptions(int argc, char** argv)
    {
        ProcessorOptions options;

        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg == "--config" && i + 1 < argc)
            {
                options.configPath = argv[++i];
            }
        }

        if (options.configPath.empty())
        {
            options.configPath = getConfigPath();
        }

        return options;
    }

    const char* queueFullPolicyName(mdp::config::QueueFullPolicy policy)
    {
        switch (policy)
        {
        case mdp::config::QueueFullPolicy::BlockSubmitter:
            return "block_submitter";
        case mdp::config::QueueFullPolicy::DropIncoming:
            return "drop_incoming";
        }

        return "unknown";
    }

    const char* queueTypeName(mdp::config::QueueType queueType)
    {
        switch (queueType)
        {
        case mdp::config::QueueType::BlockingBounded:
            return "blocking_bounded";
        case mdp::config::QueueType::LockFreeRing:
            return "lock_free_ring";
        }

        return "unknown";
    }

    void printStartupConfig(const std::filesystem::path& configPath)
    {
        const auto& config = mdp::config::get();

        std::cout << "Initializing market_data_processor" << std::endl;
        std::cout << "Config file: " << configPath << std::endl;
        std::cout << "Runtime config:" << std::endl;
        std::cout << "  app_runtime_seconds="
            << config.runtime().appRuntimeSeconds << std::endl;
        std::cout << "  num_workers=" << config.runtime().numWorkers << std::endl;
        std::cout << "  periodic_summary_interval_ms="
            << config.runtime().periodicSummaryIntervalMs << std::endl;
        std::cout << "Worker config:" << std::endl;
        std::cout << "  processing_delay_us="
            << config.worker().processingDelayUs << std::endl;
        std::cout << "  optional_busy_work_iterations="
            << config.worker().optionalBusyWorkIterations << std::endl;
        std::cout << "  queue_capacity="
            << config.worker().workerQueueCapacity << std::endl;
        std::cout << "  queue_full_policy="
            << queueFullPolicyName(config.worker().workerQueueFullStrategy)
            << std::endl;
        std::cout << "  queue_type="
            << queueTypeName(config.worker().workerQueueType) << std::endl;
        std::cout << "Network config:" << std::endl;
        std::cout << "  listen_address="
            << config.network().listenAddress << std::endl;
        std::cout << "  listen_port=" << config.network().listenPort << std::endl;
        std::cout << "  max_batch_size="
            << config.network().maxBatchSize << std::endl;
        std::cout << "  max_connections="
            << config.network().maxConnections << std::endl;
        std::cout << "Reporting config:" << std::endl;
        std::cout << "  print_processing_stats_summary="
            << (config.reporting().printProcessingStatsSummary ? "true" : "false")
            << std::endl;
        std::cout << "  print_queue_metrics_summary="
            << (config.reporting().printQueueMetricsSummary ? "true" : "false")
            << std::endl;
        std::cout << "  print_symbol_stats_summary="
            << (config.reporting().printSymbolStatsSummary ? "true" : "false")
            << std::endl;
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

    void printPeriodicSummary(
        const mdp::network::TcpEventReceiver& receiver,
        const mdp::pipeline::WorkerPool& workerPool,
        ThroughputSample& previousSample,
        const std::chrono::steady_clock::time_point& previousTime,
        const std::chrono::steady_clock::time_point& currentTime)
    {
        const auto partitionMetrics = workerPool.getPartitionMetrics();
        const std::uint64_t receivedCount = receiver.getReceivedEventCount();
        const std::size_t rejectedCount =
            static_cast<std::size_t>(receiver.getRejectedEventCount());
        const std::uint64_t processedCount = workerPool.getProcessedCount();

        const double elapsedSeconds =
            std::chrono::duration_cast<std::chrono::duration<double>>(
                currentTime - previousTime).count();

        const std::uint64_t receivedDelta = receivedCount - previousSample.receivedCount;
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

        std::cout << "[summary] received/sec=" << perSecond(receivedDelta, elapsedSeconds)
            << " processed/sec=" << perSecond(processedDelta, elapsedSeconds)
            << " rejected/sec=" << perSecond(rejectedDelta, elapsedSeconds)
            << " queueDepth=" << totalDepth << '/' << totalCapacity
            << " maxDepthSeen=" << maxDepthSeen
            << " nearCapacityQueues=" << nearCapacityQueues << '/' << partitionMetrics.size()
            << " saturatedQueues=" << saturatedQueues << '/' << partitionMetrics.size()
            << std::endl;

        previousSample.receivedCount = receivedCount;
        previousSample.rejectedCount = rejectedCount;
        previousSample.processedCount = processedCount;
    }

    void runForConfiguredDuration(
        const mdp::network::TcpEventReceiver& receiver,
        const mdp::pipeline::WorkerPool& workerPool)
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
                receiver,
                workerPool,
                previousSample,
                previousSummaryTime,
                summaryTime);
            previousSummaryTime = summaryTime;
        }
    }

    void printProcessingSummary(
        const mdp::network::TcpEventReceiver& receiver,
        const mdp::pipeline::WorkerPool& workerPool,
        double elapsedSeconds)
    {
        if (!mdp::config::get().reporting().printProcessingStatsSummary)
        {
            return;
        }

        const std::uint64_t receivedCount = receiver.getReceivedEventCount();
        const std::uint64_t submittedCount = receiver.getSubmittedEventCount();
        const std::uint64_t rejectedCount = receiver.getRejectedEventCount();
        const std::uint64_t processedCount = workerPool.getProcessedCount();

        std::cout << "Accepted connections: "
            << receiver.getAcceptedConnectionCount() << std::endl;
        std::cout << "Received count: " << receivedCount << std::endl;
        std::cout << "Submitted count: " << submittedCount << std::endl;
        std::cout << "Rejected count: " << rejectedCount << std::endl;
        std::cout << "Decode failures: "
            << receiver.getDecodeFailureCount() << std::endl;
        std::cout << "Rejected messages: "
            << receiver.getRejectedMessageCount() << std::endl;
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
        std::cout << "Received throughput: "
            << perSecond(receivedCount, elapsedSeconds) << " events/sec" << std::endl;
        std::cout << "Submitted throughput: "
            << perSecond(submittedCount, elapsedSeconds) << " events/sec" << std::endl;
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

int main(int argc, char** argv)
{
    const ProcessorOptions options = parseOptions(argc, argv);
    mdp::config::loadFromFile(options.configPath);
    printStartupConfig(options.configPath);

    mdp::pipeline::WorkerPool workerPool(mdp::config::get().runtime().numWorkers);
    mdp::network::TcpEventReceiverOptions receiverOptions;
    receiverOptions.listenAddress = mdp::config::get().network().listenAddress;
    receiverOptions.listenPort = mdp::config::get().network().listenPort;
    receiverOptions.maxBatchSize = mdp::config::get().network().maxBatchSize;
    receiverOptions.maxConnections = mdp::config::get().network().maxConnections;
    mdp::network::TcpEventReceiver receiver(workerPool, receiverOptions);

    std::cout << "Starting processor pipeline..." << std::endl;
    std::cout << "Listening on " << receiverOptions.listenAddress
        << ':' << receiverOptions.listenPort
        << " max_connections=" << receiverOptions.maxConnections << std::endl;

    workerPool.start();
    receiver.start();
    std::cout << "Processor startup complete." << std::endl;

    const auto startTime = std::chrono::steady_clock::now();
    runForConfiguredDuration(receiver, workerPool);

    std::cout << "Processor runtime elapsed; shutdown requested." << std::endl;
    std::cout << "Stopping TCP receiver and worker pool..." << std::endl;

    receiver.requestStop();
    workerPool.stop();
    receiver.join();
    workerPool.join();
    std::cout << "Processor shutdown complete." << std::endl;

    const auto endTime = std::chrono::steady_clock::now();
    const auto elapsedMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime).count();
    const double elapsedSeconds = static_cast<double>(elapsedMs) / 1000.0;

    std::cout << "Elapsed time: " << elapsedSeconds << " seconds" << std::endl;

    printProcessingSummary(receiver, workerPool, elapsedSeconds);
    printQueueSummary(workerPool);
    printSymbolSummary(workerPool);

    return 0;
}
