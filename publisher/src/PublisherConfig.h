#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace mdp::publisher::config
{
    struct RuntimeConfig
    {
        std::size_t eventCount{ 100000 };
        std::uint32_t runtimeSeconds{ 0 };
        std::size_t burstSize{ 100 };
        std::uint32_t sleepUs{ 0 };
    };

    struct NetworkConfig
    {
        std::string processorHost{ "127.0.0.1" };
        std::uint16_t processorPort{ 19000 };
        std::uint32_t connectRetryMs{ 1000 };
        std::size_t ackWindowBatches{ 1 };
    };

    struct SourceConfig
    {
        std::string sourceType{ "synthetic" };
        std::size_t symbolCount{ 32 };
        std::size_t symbolOffset{ 0 };
    };

    struct ExportConfig
    {
        bool enablePublisherMetricsCsv{ true };
        std::filesystem::path publisherMetricsCsvPath{ "results/publisher-metrics.csv" };
        std::uint32_t metricsIntervalMs{ 1000 };
    };

    class PublisherConfig
    {
    public:
        static PublisherConfig loadFromIni(const std::filesystem::path& filePath);

        const RuntimeConfig& runtime() const { return m_runtime; }
        const NetworkConfig& network() const { return m_network; }
        const SourceConfig& source() const { return m_source; }
        const ExportConfig& exportConfig() const { return m_export; }

    private:
        RuntimeConfig m_runtime;
        NetworkConfig m_network;
        SourceConfig m_source;
        ExportConfig m_export;
    };
}
