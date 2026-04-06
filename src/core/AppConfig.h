// AppConfig.h
#pragma once

#include <cstddef>
#include <cstdint>

namespace mdp::config
{
    enum class QueueFullPolicy
    {
        BlockProducer,
        DropIncoming
    };

    inline bool enableEventLogging = true;
    inline bool enableAlertLogging = true;
    inline bool enableProcessingStatsLogging = true;

    inline std::size_t processingStatsLogInterval = 1000;
    inline std::uint32_t sourceSleepMs = 1;
    inline std::size_t appRuntimeMs = 10;
    inline std::size_t numOfWorkers = 1;

    inline std::size_t workerQueueCapacity = 1024;
    inline QueueFullPolicy workerQueueFullStrategy = QueueFullPolicy::DropIncoming;

    inline std::size_t processingSpinIterations = 0;

    inline bool printProcessingStatsSummary = true;
    inline bool printQueueMetricsSummary = true;
    inline bool printSymbolStatsSummary = true;
}
