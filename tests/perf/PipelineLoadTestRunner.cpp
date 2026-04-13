#include "core/AppConfig.h"
#include "pipeline/Producer.h"
#include "pipeline/WorkerPool.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{
    struct RunnerOptions
    {
        std::vector<std::filesystem::path> configPaths;
        std::filesystem::path configDir;
        std::filesystem::path outputPath{ "results/performance-load-results.csv" };
        std::filesystem::path samplesOutputPath{ "results/performance-load-samples.csv" };
        std::size_t repeatCount{ 1 };
        std::size_t sampleIntervalMs{ 100 };
    };

    struct QueueSampleStats
    {
        std::size_t sampleCount{ 0 };
        std::size_t nearCapacitySamples{ 0 };
        std::size_t maxTotalDepth{ 0 };
    };

    struct RunResult
    {
        std::string profile;
        std::size_t runIndex{ 0 };
        double durationSec{ 0.0 };
        std::size_t workerCount{ 0 };
        std::size_t producerCount{ 0 };
        std::size_t activeProducerCount{ 0 };
        std::size_t producerBurstSize{ 0 };
        std::uint32_t producerSleepUs{ 0 };
        std::uint32_t processingDelayUs{ 0 };
        std::size_t busyWorkIterations{ 0 };
        std::size_t workerQueueCapacity{ 0 };
        std::string queueFullPolicy;
        std::string queueType;
        std::uint64_t eventsGenerated{ 0 };
        std::uint64_t eventsAccepted{ 0 };
        std::uint64_t eventsRejected{ 0 };
        std::uint64_t eventsProcessed{ 0 };
        std::uint64_t validEvents{ 0 };
        std::uint64_t invalidEvents{ 0 };
        std::uint64_t duplicateEvents{ 0 };
        std::uint64_t outOfOrderEvents{ 0 };
        std::uint64_t sequenceGaps{ 0 };
        std::uint64_t queueDropped{ 0 };
        std::uint64_t queueFailedEnqueue{ 0 };
        std::size_t queueCurrentDepthTotal{ 0 };
        std::size_t queueMaxDepthSeen{ 0 };
        std::size_t queueMaxTotalDepthSampled{ 0 };
        std::size_t queueCapacityTotal{ 0 };
        double nearCapacitySamplePercent{ 0.0 };
        double generatedPerSec{ 0.0 };
        double acceptedPerSec{ 0.0 };
        double processedPerSec{ 0.0 };
        std::uint64_t latencyAverageNs{ 0 };
        std::uint64_t latencyMinNs{ 0 };
        std::uint64_t latencyMaxNs{ 0 };
        std::uint64_t latencyP50Ns{ 0 };
        std::uint64_t latencyP95Ns{ 0 };
        std::uint64_t latencyP99Ns{ 0 };
        std::uint64_t queueWaitAverageNs{ 0 };
        std::uint64_t queueWaitP50Ns{ 0 };
        std::uint64_t queueWaitP95Ns{ 0 };
        std::uint64_t queueWaitP99Ns{ 0 };
    };

    struct IntervalSample
    {
        std::string profile;
        std::size_t runIndex{ 0 };
        double elapsedSec{ 0.0 };
        double intervalSec{ 0.0 };
        std::uint64_t generatedDelta{ 0 };
        std::uint64_t acceptedDelta{ 0 };
        std::uint64_t rejectedDelta{ 0 };
        std::uint64_t processedDelta{ 0 };
        std::uint64_t validDelta{ 0 };
        std::uint64_t invalidDelta{ 0 };
        std::uint64_t duplicateDelta{ 0 };
        std::uint64_t outOfOrderDelta{ 0 };
        std::uint64_t sequenceGapDelta{ 0 };
        std::uint64_t queueDroppedDelta{ 0 };
        std::uint64_t queueFailedEnqueueDelta{ 0 };
        double generatedPerSec{ 0.0 };
        double acceptedPerSec{ 0.0 };
        double rejectedPerSec{ 0.0 };
        double processedPerSec{ 0.0 };
        std::size_t queueCurrentDepthTotal{ 0 };
        std::size_t queueCapacityTotal{ 0 };
        std::size_t queueMaxDepthSeen{ 0 };
        std::size_t nearCapacityQueues{ 0 };
        double queueDepthPercent{ 0.0 };
        std::uint64_t latencySampleCount{ 0 };
        std::uint64_t latencyP50Ns{ 0 };
        std::uint64_t latencyP95Ns{ 0 };
        std::uint64_t latencyP99Ns{ 0 };
        std::uint64_t queueWaitSampleCount{ 0 };
        std::uint64_t queueWaitP50Ns{ 0 };
        std::uint64_t queueWaitP95Ns{ 0 };
        std::uint64_t queueWaitP99Ns{ 0 };
    };

    struct CounterSnapshot
    {
        std::uint64_t generated{ 0 };
        std::uint64_t accepted{ 0 };
        std::uint64_t rejected{ 0 };
        std::uint64_t processed{ 0 };
        std::uint64_t valid{ 0 };
        std::uint64_t invalid{ 0 };
        std::uint64_t duplicate{ 0 };
        std::uint64_t outOfOrder{ 0 };
        std::uint64_t sequenceGap{ 0 };
        std::uint64_t queueDropped{ 0 };
        std::uint64_t queueFailedEnqueue{ 0 };
    };

    struct QueueTotals
    {
        std::size_t currentDepth{ 0 };
        std::size_t capacity{ 0 };
        std::size_t maxDepthSeen{ 0 };
        std::size_t nearCapacityQueues{ 0 };
        std::uint64_t dropped{ 0 };
        std::uint64_t failedEnqueue{ 0 };
    };

    std::filesystem::path repoRoot()
    {
        return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
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

    std::string queuePolicyToString(mdp::config::QueueFullPolicy policy)
    {
        switch (policy)
        {
        case mdp::config::QueueFullPolicy::BlockProducer:
            return "block_producer";
        case mdp::config::QueueFullPolicy::DropIncoming:
            return "drop_incoming";
        }

        return "unknown";
    }

    std::string queueTypeToString(mdp::config::QueueType queueType)
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

    void printUsage()
    {
        std::cout
            << "Usage:\n"
            << "  pipeline_load_experiments --config <file.ini> --out <results.csv> --samples-out <samples.csv>\n"
            << "  pipeline_load_experiments --config-dir <dir> --out <results.csv> --samples-out <samples.csv> --repeat 3\n";
    }

    RunnerOptions parseArgs(int argc, char** argv)
    {
        RunnerOptions options;

        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];

            if (arg == "--help" || arg == "-h")
            {
                printUsage();
                std::exit(0);
            }

            if (i + 1 >= argc)
            {
                throw std::runtime_error("Missing value for argument: " + arg);
            }

            const std::string value = argv[++i];

            if (arg == "--config")
            {
                options.configPaths.push_back(value);
            }
            else if (arg == "--config-dir")
            {
                options.configDir = value;
            }
            else if (arg == "--out")
            {
                options.outputPath = value;
            }
            else if (arg == "--samples-out")
            {
                options.samplesOutputPath = value;
            }
            else if (arg == "--repeat")
            {
                options.repeatCount = std::stoull(value);
            }
            else if (arg == "--sample-ms")
            {
                options.sampleIntervalMs = std::stoull(value);
            }
            else
            {
                throw std::runtime_error("Unknown argument: " + arg);
            }
        }

        if (options.configPaths.empty() && options.configDir.empty())
        {
            options.configDir = repoRoot() / "tests" / "perf" / "configs";
        }

        options.repeatCount = std::max<std::size_t>(1, options.repeatCount);
        options.sampleIntervalMs = std::max<std::size_t>(1, options.sampleIntervalMs);
        return options;
    }

    std::vector<std::filesystem::path> collectConfigFiles(const RunnerOptions& options)
    {
        std::vector<std::filesystem::path> configs;

        for (const auto& configPath : options.configPaths)
        {
            configs.push_back(configPath);
        }

        if (!options.configDir.empty())
        {
            for (const auto& entry : std::filesystem::directory_iterator(options.configDir))
            {
                if (entry.is_regular_file() && entry.path().extension() == ".ini")
                {
                    configs.push_back(entry.path());
                }
            }
        }

        std::sort(configs.begin(), configs.end());
        return configs;
    }

    std::uint64_t sumLatencyBuckets(const mdp::metrics::LatencyRecorder::BucketSnapshot& buckets)
    {
        std::uint64_t total = 0;

        for (const auto count : buckets)
        {
            total += count;
        }

        return total;
    }

    mdp::metrics::LatencyRecorder::BucketSnapshot subtractLatencyBuckets(
        const mdp::metrics::LatencyRecorder::BucketSnapshot& current,
        const mdp::metrics::LatencyRecorder::BucketSnapshot& previous)
    {
        mdp::metrics::LatencyRecorder::BucketSnapshot delta{};

        for (std::size_t i = 0; i < current.size(); ++i)
        {
            delta[i] = current[i] >= previous[i] ? current[i] - previous[i] : 0;
        }

        return delta;
    }

    QueueTotals getQueueTotals(const mdp::pipeline::WorkerPool& workerPool)
    {
        QueueTotals totals;
        const auto partitionMetrics = workerPool.getPartitionMetrics();

        for (const auto& metrics : partitionMetrics)
        {
            totals.currentDepth += metrics.currentDepth;
            totals.capacity += metrics.capacity;
            totals.maxDepthSeen = std::max(totals.maxDepthSeen, metrics.maxDepth);
            totals.dropped += metrics.droppedCount;
            totals.failedEnqueue += metrics.failedEnqueueCount;

            if (isNearCapacity(metrics))
            {
                ++totals.nearCapacityQueues;
            }
        }

        return totals;
    }

    CounterSnapshot getCounterSnapshot(
        const mdp::pipeline::Producer& producer,
        const mdp::pipeline::WorkerPool& workerPool)
    {
        const QueueTotals queueTotals = getQueueTotals(workerPool);
        CounterSnapshot snapshot;
        snapshot.accepted = producer.getProducedCount();
        snapshot.rejected = producer.getRejectedCount();
        snapshot.generated = snapshot.accepted + snapshot.rejected;
        snapshot.processed = workerPool.getProcessedCount();
        snapshot.valid = workerPool.getValidCount();
        snapshot.invalid = workerPool.getInvalidCount();
        snapshot.duplicate = workerPool.getDuplicateCount();
        snapshot.outOfOrder = workerPool.getOutOfOrderCount();
        snapshot.sequenceGap = workerPool.getSequenceGapCount();
        snapshot.queueDropped = queueTotals.dropped;
        snapshot.queueFailedEnqueue = queueTotals.failedEnqueue;
        return snapshot;
    }

    QueueSampleStats collectIntervalSamples(
        const std::string& profile,
        std::size_t runIndex,
        const mdp::pipeline::Producer& producer,
        const mdp::pipeline::WorkerPool& workerPool,
        std::chrono::steady_clock::time_point startTime,
        std::chrono::steady_clock::time_point endTime,
        std::size_t sampleIntervalMs,
        std::vector<IntervalSample>& samples)
    {
        QueueSampleStats queueSampleStats;
        CounterSnapshot previousCounters = getCounterSnapshot(producer, workerPool);
        mdp::metrics::LatencyRecorder::BucketSnapshot previousLatencyBuckets =
            workerPool.getLatencyBucketSnapshot();
        mdp::metrics::LatencyRecorder::BucketSnapshot previousQueueWaitBuckets =
            workerPool.getQueueWaitLatencyBucketSnapshot();
        auto previousTime = startTime;

        while (std::chrono::steady_clock::now() < endTime)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(sampleIntervalMs));

            const auto now = std::chrono::steady_clock::now();
            const double elapsedSec =
                std::chrono::duration_cast<std::chrono::duration<double>>(
                    now - startTime).count();
            const double intervalSec =
                std::chrono::duration_cast<std::chrono::duration<double>>(
                    now - previousTime).count();

            const CounterSnapshot counters = getCounterSnapshot(producer, workerPool);
            const QueueTotals queueTotals = getQueueTotals(workerPool);
            const auto currentLatencyBuckets = workerPool.getLatencyBucketSnapshot();
            const auto latencyDeltaBuckets = subtractLatencyBuckets(
                currentLatencyBuckets,
                previousLatencyBuckets);
            const std::uint64_t latencyDeltaCount = sumLatencyBuckets(latencyDeltaBuckets);
            const auto currentQueueWaitBuckets =
                workerPool.getQueueWaitLatencyBucketSnapshot();
            const auto queueWaitDeltaBuckets = subtractLatencyBuckets(
                currentQueueWaitBuckets,
                previousQueueWaitBuckets);
            const std::uint64_t queueWaitDeltaCount = sumLatencyBuckets(queueWaitDeltaBuckets);

            IntervalSample sample;
            sample.profile = profile;
            sample.runIndex = runIndex;
            sample.elapsedSec = elapsedSec;
            sample.intervalSec = intervalSec;
            sample.generatedDelta = counters.generated - previousCounters.generated;
            sample.acceptedDelta = counters.accepted - previousCounters.accepted;
            sample.rejectedDelta = counters.rejected - previousCounters.rejected;
            sample.processedDelta = counters.processed - previousCounters.processed;
            sample.validDelta = counters.valid - previousCounters.valid;
            sample.invalidDelta = counters.invalid - previousCounters.invalid;
            sample.duplicateDelta = counters.duplicate - previousCounters.duplicate;
            sample.outOfOrderDelta = counters.outOfOrder - previousCounters.outOfOrder;
            sample.sequenceGapDelta = counters.sequenceGap - previousCounters.sequenceGap;
            sample.queueDroppedDelta = counters.queueDropped - previousCounters.queueDropped;
            sample.queueFailedEnqueueDelta =
                counters.queueFailedEnqueue - previousCounters.queueFailedEnqueue;
            sample.generatedPerSec = perSecond(sample.generatedDelta, intervalSec);
            sample.acceptedPerSec = perSecond(sample.acceptedDelta, intervalSec);
            sample.rejectedPerSec = perSecond(sample.rejectedDelta, intervalSec);
            sample.processedPerSec = perSecond(sample.processedDelta, intervalSec);
            sample.queueCurrentDepthTotal = queueTotals.currentDepth;
            sample.queueCapacityTotal = queueTotals.capacity;
            sample.queueMaxDepthSeen = queueTotals.maxDepthSeen;
            sample.nearCapacityQueues = queueTotals.nearCapacityQueues;
            sample.queueDepthPercent =
                queueTotals.capacity == 0
                ? 0.0
                : 100.0 * static_cast<double>(queueTotals.currentDepth) /
                    static_cast<double>(queueTotals.capacity);
            sample.latencySampleCount = latencyDeltaCount;
            sample.latencyP50Ns = mdp::metrics::LatencyRecorder::percentileFromBuckets(
                latencyDeltaBuckets,
                latencyDeltaCount,
                50.0);
            sample.latencyP95Ns = mdp::metrics::LatencyRecorder::percentileFromBuckets(
                latencyDeltaBuckets,
                latencyDeltaCount,
                95.0);
            sample.latencyP99Ns = mdp::metrics::LatencyRecorder::percentileFromBuckets(
                latencyDeltaBuckets,
                latencyDeltaCount,
                99.0);
            sample.queueWaitSampleCount = queueWaitDeltaCount;
            sample.queueWaitP50Ns = mdp::metrics::LatencyRecorder::percentileFromBuckets(
                queueWaitDeltaBuckets,
                queueWaitDeltaCount,
                50.0);
            sample.queueWaitP95Ns = mdp::metrics::LatencyRecorder::percentileFromBuckets(
                queueWaitDeltaBuckets,
                queueWaitDeltaCount,
                95.0);
            sample.queueWaitP99Ns = mdp::metrics::LatencyRecorder::percentileFromBuckets(
                queueWaitDeltaBuckets,
                queueWaitDeltaCount,
                99.0);

            samples.push_back(sample);

            queueSampleStats.maxTotalDepth =
                std::max(queueSampleStats.maxTotalDepth, queueTotals.currentDepth);
            queueSampleStats.sampleCount += workerPool.getWorkerCount();
            queueSampleStats.nearCapacitySamples += queueTotals.nearCapacityQueues;

            previousCounters = counters;
            previousLatencyBuckets = currentLatencyBuckets;
            previousQueueWaitBuckets = currentQueueWaitBuckets;
            previousTime = now;
        }

        return queueSampleStats;
    }

    RunResult runConfig(
        const std::filesystem::path& configPath,
        std::size_t runIndex,
        std::size_t sampleIntervalMs,
        std::vector<IntervalSample>& intervalSamples)
    {
        mdp::config::loadFromFile(configPath);

        const auto& config = mdp::config::get();
        const std::string profile = configPath.stem().string();
        mdp::pipeline::WorkerPool workerPool(config.runtime().numWorkers);
        mdp::pipeline::Producer producer(workerPool);

        workerPool.start();
        producer.start();

        const auto startTime = std::chrono::steady_clock::now();
        const auto endTime = startTime + std::chrono::seconds(config.runtime().appRuntimeSeconds);
        const QueueSampleStats queueSamples = collectIntervalSamples(
            profile,
            runIndex,
            producer,
            workerPool,
            startTime,
            endTime,
            sampleIntervalMs,
            intervalSamples);

        const auto finishTime = std::chrono::steady_clock::now();
        const double durationSec =
            std::chrono::duration_cast<std::chrono::duration<double>>(
                finishTime - startTime).count();
        const std::size_t acceptedCount = producer.getProducedCount();
        const std::size_t rejectedCount = producer.getRejectedCount();
        const auto partitionMetrics = workerPool.getPartitionMetrics();
        const std::uint64_t processedCount = workerPool.getProcessedCount();
        const std::uint64_t validCount = workerPool.getValidCount();
        const std::uint64_t invalidCount = workerPool.getInvalidCount();
        const std::uint64_t duplicateCount = workerPool.getDuplicateCount();
        const std::uint64_t outOfOrderCount = workerPool.getOutOfOrderCount();
        const std::uint64_t sequenceGapCount = workerPool.getSequenceGapCount();
        const std::uint64_t latencyAverageNs = workerPool.getAverageLatencyNs();
        const std::uint64_t latencyMinNs = workerPool.getMinLatencyNs();
        const std::uint64_t latencyMaxNs = workerPool.getMaxLatencyNs();
        const std::uint64_t latencyP50Ns = workerPool.getPercentileLatencyNs(50.0);
        const std::uint64_t latencyP95Ns = workerPool.getPercentileLatencyNs(95.0);
        const std::uint64_t latencyP99Ns = workerPool.getPercentileLatencyNs(99.0);
        const std::uint64_t queueWaitAverageNs = workerPool.getAverageQueueWaitLatencyNs();
        const std::uint64_t queueWaitP50Ns = workerPool.getPercentileQueueWaitLatencyNs(50.0);
        const std::uint64_t queueWaitP95Ns = workerPool.getPercentileQueueWaitLatencyNs(95.0);
        const std::uint64_t queueWaitP99Ns = workerPool.getPercentileQueueWaitLatencyNs(99.0);

        producer.requestStop();
        workerPool.stop(false);
        producer.join();
        workerPool.join();

        RunResult result;
        result.profile = profile;
        result.runIndex = runIndex;
        result.durationSec = durationSec;
        result.workerCount = config.runtime().numWorkers;
        result.producerCount = config.producer().producerCount;
        result.activeProducerCount = producer.getActiveProducerCount();
        result.producerBurstSize = config.producer().producerBurstSize;
        result.producerSleepUs = config.producer().producerSleepUs;
        result.processingDelayUs = config.worker().processingDelayUs;
        result.busyWorkIterations = config.worker().optionalBusyWorkIterations;
        result.workerQueueCapacity = config.worker().workerQueueCapacity;
        result.queueFullPolicy = queuePolicyToString(config.worker().workerQueueFullStrategy);
        result.queueType = queueTypeToString(config.worker().workerQueueType);
        result.eventsAccepted = acceptedCount;
        result.eventsRejected = rejectedCount;
        result.eventsGenerated = result.eventsAccepted + result.eventsRejected;
        result.eventsProcessed = processedCount;
        result.validEvents = validCount;
        result.invalidEvents = invalidCount;
        result.duplicateEvents = duplicateCount;
        result.outOfOrderEvents = outOfOrderCount;
        result.sequenceGaps = sequenceGapCount;
        result.queueMaxTotalDepthSampled = queueSamples.maxTotalDepth;
        result.nearCapacitySamplePercent =
            queueSamples.sampleCount == 0
            ? 0.0
            : (100.0 * static_cast<double>(queueSamples.nearCapacitySamples) /
                static_cast<double>(queueSamples.sampleCount));
        result.generatedPerSec = perSecond(result.eventsGenerated, durationSec);
        result.acceptedPerSec = perSecond(result.eventsAccepted, durationSec);
        result.processedPerSec = perSecond(result.eventsProcessed, durationSec);
        result.latencyAverageNs = latencyAverageNs;
        result.latencyMinNs = latencyMinNs;
        result.latencyMaxNs = latencyMaxNs;
        result.latencyP50Ns = latencyP50Ns;
        result.latencyP95Ns = latencyP95Ns;
        result.latencyP99Ns = latencyP99Ns;
        result.queueWaitAverageNs = queueWaitAverageNs;
        result.queueWaitP50Ns = queueWaitP50Ns;
        result.queueWaitP95Ns = queueWaitP95Ns;
        result.queueWaitP99Ns = queueWaitP99Ns;

        for (const auto& metrics : partitionMetrics)
        {
            result.queueDropped += metrics.droppedCount;
            result.queueFailedEnqueue += metrics.failedEnqueueCount;
            result.queueCurrentDepthTotal += metrics.currentDepth;
            result.queueMaxDepthSeen = std::max(result.queueMaxDepthSeen, metrics.maxDepth);
            result.queueCapacityTotal += metrics.capacity;
        }

        return result;
    }

    void writeCsvHeader(std::ostream& output)
    {
        output
            << "profile,run,durationSec,workerCount,producerCount,activeProducerCount,"
            << "producerBurstSize,producerSleepUs,processingDelayUs,busyWorkIterations,"
            << "workerQueueCapacity,queueFullPolicy,queueType,eventsGenerated,eventsAccepted,"
            << "eventsRejected,eventsProcessed,validEvents,invalidEvents,duplicateEvents,"
            << "outOfOrderEvents,sequenceGaps,queueDropped,queueFailedEnqueue,"
            << "queueCurrentDepthTotal,queueMaxDepthSeen,queueMaxTotalDepthSampled,"
            << "queueCapacityTotal,nearCapacitySamplePercent,generatedPerSec,"
            << "acceptedPerSec,processedPerSec,latencyAverageNs,latencyMinNs,"
            << "latencyMaxNs,latencyP50Ns,latencyP95Ns,latencyP99Ns,"
            << "queueWaitAverageNs,queueWaitP50Ns,queueWaitP95Ns,queueWaitP99Ns\n";
    }

    void writeIntervalSampleHeader(std::ostream& output)
    {
        output
            << "profile,run,elapsedSec,intervalSec,generatedDelta,acceptedDelta,"
            << "rejectedDelta,processedDelta,validDelta,invalidDelta,duplicateDelta,"
            << "outOfOrderDelta,sequenceGapDelta,queueDroppedDelta,"
            << "queueFailedEnqueueDelta,generatedPerSec,acceptedPerSec,"
            << "rejectedPerSec,processedPerSec,queueCurrentDepthTotal,"
            << "queueCapacityTotal,queueMaxDepthSeen,nearCapacityQueues,"
            << "queueDepthPercent,latencySampleCount,latencyP50Ns,latencyP95Ns,"
            << "latencyP99Ns,queueWaitSampleCount,queueWaitP50Ns,queueWaitP95Ns,"
            << "queueWaitP99Ns\n";
    }

    void writeCsvRow(std::ostream& output, const RunResult& result)
    {
        output << result.profile
            << ',' << result.runIndex
            << ',' << std::fixed << std::setprecision(6) << result.durationSec
            << ',' << result.workerCount
            << ',' << result.producerCount
            << ',' << result.activeProducerCount
            << ',' << result.producerBurstSize
            << ',' << result.producerSleepUs
            << ',' << result.processingDelayUs
            << ',' << result.busyWorkIterations
            << ',' << result.workerQueueCapacity
            << ',' << result.queueFullPolicy
            << ',' << result.queueType
            << ',' << result.eventsGenerated
            << ',' << result.eventsAccepted
            << ',' << result.eventsRejected
            << ',' << result.eventsProcessed
            << ',' << result.validEvents
            << ',' << result.invalidEvents
            << ',' << result.duplicateEvents
            << ',' << result.outOfOrderEvents
            << ',' << result.sequenceGaps
            << ',' << result.queueDropped
            << ',' << result.queueFailedEnqueue
            << ',' << result.queueCurrentDepthTotal
            << ',' << result.queueMaxDepthSeen
            << ',' << result.queueMaxTotalDepthSampled
            << ',' << result.queueCapacityTotal
            << ',' << std::fixed << std::setprecision(2) << result.nearCapacitySamplePercent
            << ',' << std::fixed << std::setprecision(2) << result.generatedPerSec
            << ',' << result.acceptedPerSec
            << ',' << result.processedPerSec
            << ',' << result.latencyAverageNs
            << ',' << result.latencyMinNs
            << ',' << result.latencyMaxNs
            << ',' << result.latencyP50Ns
            << ',' << result.latencyP95Ns
            << ',' << result.latencyP99Ns
            << ',' << result.queueWaitAverageNs
            << ',' << result.queueWaitP50Ns
            << ',' << result.queueWaitP95Ns
            << ',' << result.queueWaitP99Ns
            << '\n';
    }

    void writeIntervalSampleRow(std::ostream& output, const IntervalSample& sample)
    {
        output << sample.profile
            << ',' << sample.runIndex
            << ',' << std::fixed << std::setprecision(6) << sample.elapsedSec
            << ',' << sample.intervalSec
            << ',' << sample.generatedDelta
            << ',' << sample.acceptedDelta
            << ',' << sample.rejectedDelta
            << ',' << sample.processedDelta
            << ',' << sample.validDelta
            << ',' << sample.invalidDelta
            << ',' << sample.duplicateDelta
            << ',' << sample.outOfOrderDelta
            << ',' << sample.sequenceGapDelta
            << ',' << sample.queueDroppedDelta
            << ',' << sample.queueFailedEnqueueDelta
            << ',' << std::fixed << std::setprecision(2) << sample.generatedPerSec
            << ',' << sample.acceptedPerSec
            << ',' << sample.rejectedPerSec
            << ',' << sample.processedPerSec
            << ',' << sample.queueCurrentDepthTotal
            << ',' << sample.queueCapacityTotal
            << ',' << sample.queueMaxDepthSeen
            << ',' << sample.nearCapacityQueues
            << ',' << std::fixed << std::setprecision(2) << sample.queueDepthPercent
            << ',' << sample.latencySampleCount
            << ',' << sample.latencyP50Ns
            << ',' << sample.latencyP95Ns
            << ',' << sample.latencyP99Ns
            << ',' << sample.queueWaitSampleCount
            << ',' << sample.queueWaitP50Ns
            << ',' << sample.queueWaitP95Ns
            << ',' << sample.queueWaitP99Ns
            << '\n';
    }
}

int main(int argc, char** argv)
{
    try
    {
        const RunnerOptions options = parseArgs(argc, argv);
        const auto configs = collectConfigFiles(options);

        if (configs.empty())
        {
            throw std::runtime_error("No .ini config files found");
        }

        if (options.outputPath.has_parent_path())
        {
            std::filesystem::create_directories(options.outputPath.parent_path());
        }

        if (options.samplesOutputPath.has_parent_path())
        {
            std::filesystem::create_directories(options.samplesOutputPath.parent_path());
        }

        std::ofstream output(options.outputPath, std::ios::trunc);
        if (!output.is_open())
        {
            throw std::runtime_error("Failed to open output file: " + options.outputPath.string());
        }

        std::ofstream samplesOutput(options.samplesOutputPath, std::ios::trunc);
        if (!samplesOutput.is_open())
        {
            throw std::runtime_error(
                "Failed to open samples output file: " + options.samplesOutputPath.string());
        }

        writeCsvHeader(output);
        writeIntervalSampleHeader(samplesOutput);

        for (const auto& configPath : configs)
        {
            for (std::size_t runIndex = 1; runIndex <= options.repeatCount; ++runIndex)
            {
                std::cout << "Running " << configPath << " run " << runIndex << '\n';
                std::vector<IntervalSample> intervalSamples;
                const RunResult result = runConfig(
                    configPath,
                    runIndex,
                    options.sampleIntervalMs,
                    intervalSamples);
                writeCsvRow(output, result);

                for (const auto& sample : intervalSamples)
                {
                    writeIntervalSampleRow(samplesOutput, sample);
                }
            }
        }

        std::cout << "Wrote results to " << options.outputPath << '\n';
        std::cout << "Wrote interval samples to " << options.samplesOutputPath << '\n';
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "pipeline_load_experiments failed: " << ex.what() << '\n';
        return 1;
    }
}
