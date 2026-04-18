// AppConfig.h
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace mdp::config
{
    enum class QueueFullPolicy
    {
        BlockSubmitter,
        DropIncoming
    };

    enum class QueueType
    {
        BlockingBounded,
        LockFreeRing
    };

    struct LoggingConfig
    {
        bool enableEventLogging{ true };
        bool enableAlertLogging{ true };
        bool enableProcessingStatsLogging{ true };
    };

    struct RuntimeConfig
    {
        std::size_t appRuntimeSeconds{ 10 };
        std::size_t numWorkers{ 1 };
        std::size_t periodicSummaryIntervalMs{ 1000 };
    };

    struct WorkerConfig
    {
        std::uint32_t processingDelayUs{ 0 };
        std::size_t optionalBusyWorkIterations{ 0 };
        std::size_t workerQueueCapacity{ 1024 };
        QueueFullPolicy workerQueueFullStrategy{ QueueFullPolicy::DropIncoming };
        QueueType workerQueueType{ QueueType::BlockingBounded };
    };

    struct NetworkConfig
    {
        std::string listenAddress{ "127.0.0.1" };
        std::uint16_t listenPort{ 19000 };
        std::size_t maxBatchSize{ 256 };
    };

    struct ReportingConfig
    {
        std::size_t processingStatsLogInterval{ 1000 };
        bool printProcessingStatsSummary{ true };
        bool printQueueMetricsSummary{ true };
        bool printSymbolStatsSummary{ true };
    };

    class AppConfig
    {
    public:
        static AppConfig loadFromIni(const std::filesystem::path& filePath);

        const LoggingConfig& logging() const { return m_logging; }
        const RuntimeConfig& runtime() const { return m_runtime; }
        const WorkerConfig& worker() const { return m_worker; }
        const NetworkConfig& network() const { return m_network; }
        const ReportingConfig& reporting() const { return m_reporting; }

    private:
        LoggingConfig m_logging;
        RuntimeConfig m_runtime;
        WorkerConfig m_worker;
        NetworkConfig m_network;
        ReportingConfig m_reporting;
    };

    const AppConfig& get();
    void loadFromFile(const std::filesystem::path& filePath);
}
