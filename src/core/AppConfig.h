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
        BlockProducer,
        DropIncoming
    };

    struct LoggingConfig
    {
        bool enableEventLogging{ true };
        bool enableAlertLogging{ true };
        bool enableProcessingStatsLogging{ true };
    };

    struct RuntimeConfig
    {
        bool enableLoadTestMode{ false };
        std::size_t appRuntimeSeconds{ 10 };
        std::size_t numWorkers{ 1 };
        std::size_t periodicSummaryIntervalMs{ 1000 };
    };

    struct ProducerConfig
    {
        std::size_t producerCount{ 1 };
        std::size_t producerBurstSize{ 1 };
        std::uint32_t producerSleepUs{ 1000 };
    };

    struct WorkerConfig
    {
        std::uint32_t processingDelayUs{ 0 };
        std::size_t optionalBusyWorkIterations{ 0 };
        std::size_t workerQueueCapacity{ 1024 };
        QueueFullPolicy workerQueueFullStrategy{ QueueFullPolicy::DropIncoming };
    };

    struct ReportingConfig
    {
        std::size_t processingStatsLogInterval{ 1000 };
        bool printProcessingStatsSummary{ true };
        bool printQueueMetricsSummary{ true };
        bool printSymbolStatsSummary{ true };
    };

    struct LoadTestConfig
    {
        std::size_t producerBurstSize{ 1 };
        std::uint32_t producerSleepUs{ 0 };
        std::uint32_t processingDelayUs{ 0 };
        std::size_t optionalBusyWorkIterations{ 0 };
        std::size_t workerQueueCapacity{ 1024 };
    };

    class AppConfig
    {
    public:
        static AppConfig loadFromIni(const std::filesystem::path& filePath);

        const LoggingConfig& logging() const { return m_logging; }
        const RuntimeConfig& runtime() const { return m_runtime; }
        const ProducerConfig& producer() const { return m_producer; }
        const WorkerConfig& worker() const { return m_worker; }
        const ReportingConfig& reporting() const { return m_reporting; }
        const LoadTestConfig& loadTest() const { return m_loadTest; }

    private:
        void applyLoadTestOverrides();

    private:
        LoggingConfig m_logging;
        RuntimeConfig m_runtime;
        ProducerConfig m_producer;
        WorkerConfig m_worker;
        ReportingConfig m_reporting;
        LoadTestConfig m_loadTest;
    };

    const AppConfig& get();
    void loadFromFile(const std::filesystem::path& filePath);
}
