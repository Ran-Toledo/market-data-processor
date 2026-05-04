#include "PublisherConfig.h"
#include "PublisherSourceFactory.h"
#include "TcpPublisherClient.h"

#include "api/protocol/MarketDataEventFrame.h"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{
    struct PublisherOptions
    {
        std::filesystem::path configPath;
    };

    double perSecond(std::size_t count, double elapsedSeconds)
    {
        if (elapsedSeconds <= 0.0)
        {
            return 0.0;
        }

        return static_cast<double>(count) / elapsedSeconds;
    }

    struct PublisherMetricsSample
    {
        std::size_t generatedCount{ 0 };
        std::size_t encodedCount{ 0 };
        std::size_t encodeFailureCount{ 0 };
        std::uint64_t acceptedCount{ 0 };
    };

    void ensureParentDirectory(const std::filesystem::path& path)
    {
        const auto parent = path.parent_path();
        if (!parent.empty())
        {
            std::filesystem::create_directories(parent);
        }
    }

    class PublisherMetricsCsvWriter
    {
    public:
        explicit PublisherMetricsCsvWriter(const std::filesystem::path& path)
        {
            ensureParentDirectory(path);
            m_output.open(path, std::ios::out | std::ios::trunc);

            if (!m_output.is_open())
            {
                throw std::runtime_error(
                    "Failed to open publisher metrics CSV: " + path.string());
            }

            m_output
                << "elapsedSec,intervalSec,generatedTotal,encodedTotal,"
                << "acceptedTotal,encodeFailuresTotal,generatedPerSec,"
                << "encodedPerSec,acceptedPerSec,encodeFailuresPerSec\n";
        }

        void write(
            double elapsedSeconds,
            double intervalSeconds,
            const PublisherMetricsSample& current,
            const PublisherMetricsSample& previous)
        {
            m_output
                << elapsedSeconds << ','
                << intervalSeconds << ','
                << current.generatedCount << ','
                << current.encodedCount << ','
                << current.acceptedCount << ','
                << current.encodeFailureCount << ','
                << perSecond(
                    current.generatedCount - previous.generatedCount,
                    intervalSeconds) << ','
                << perSecond(
                    current.encodedCount - previous.encodedCount,
                    intervalSeconds) << ','
                << perSecond(
                    static_cast<std::size_t>(
                        current.acceptedCount - previous.acceptedCount),
                    intervalSeconds) << ','
                << perSecond(
                    current.encodeFailureCount - previous.encodeFailureCount,
                    intervalSeconds) << '\n';
        }

    private:
        std::ofstream m_output;
    };

    PublisherOptions parseOptions(int argc, char** argv)
    {
        PublisherOptions options;

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
            options.configPath = std::filesystem::path(__FILE__)
                .parent_path()
                .parent_path()
                / "config"
                / "publisher.ini";
        }

        return options;
    }

    void printStartupConfig(
        const std::filesystem::path& configPath,
        const mdp::publisher::config::PublisherConfig& config)
    {
        std::cout << "Initializing market_data_publisher" << '\n';
        std::cout << "Config file: " << configPath << '\n';
        std::cout << "Runtime config:" << '\n';
        std::cout << "  event_count=" << config.runtime().eventCount << '\n';
        std::cout << "  runtime_seconds="
            << config.runtime().runtimeSeconds << '\n';
        std::cout << "  burst_size=" << config.runtime().burstSize << '\n';
        std::cout << "  sleep_us=" << config.runtime().sleepUs << '\n';
        std::cout << "Network config:" << '\n';
        std::cout << "  processor_host="
            << config.network().processorHost << '\n';
        std::cout << "  processor_port="
            << config.network().processorPort << '\n';
        std::cout << "  connect_retry_ms="
            << config.network().connectRetryMs << '\n';
        std::cout << "  ack_window_batches="
            << config.network().ackWindowBatches << '\n';
        std::cout << "Source config:" << '\n';
        std::cout << "  source_type=" << config.source().sourceType << '\n';
        std::cout << "  symbol_count=" << config.source().symbolCount << '\n';
        std::cout << "  symbol_offset=" << config.source().symbolOffset << '\n';
        if (config.source().sourceType == "ibkr")
        {
            std::cout << "IBKR config:" << '\n';
            std::cout << "  transport=" << config.ibkr().transport << '\n';
            std::cout << "  base_url=" << config.ibkr().baseUrl << '\n';
            std::cout << "  websocket_url=" << config.ibkr().websocketUrl << '\n';
            std::cout << "  symbols_count=" << config.ibkr().symbols.size() << '\n';
            std::cout << "  conids_count=" << config.ibkr().conids.size() << '\n';
            std::cout << "  security_type=" << config.ibkr().securityType << '\n';
            std::cout << "  fields_count=" << config.ibkr().fields.size() << '\n';
            std::cout << "  poll_interval_ms=" << config.ibkr().pollIntervalMs << '\n';
            std::cout << "  websocket_ping_interval_ms="
                << config.ibkr().websocketPingIntervalMs << '\n';
            std::cout << "  event_queue_capacity="
                << config.ibkr().eventQueueCapacity << '\n';
            std::cout << "  check_auth_on_startup="
                << (config.ibkr().checkAuthOnStartup ? "true" : "false") << '\n';
            std::cout << "  call_accounts_on_startup="
                << (config.ibkr().callAccountsOnStartup ? "true" : "false") << '\n';
            std::cout << "  allow_insecure_localhost_tls="
                << (config.ibkr().allowInsecureLocalhostTls ? "true" : "false") << '\n';
        }
        std::cout << "Export config:" << '\n';
        std::cout << "  enable_publisher_metrics_csv="
            << (config.exportConfig().enablePublisherMetricsCsv ? "true" : "false")
            << '\n';
        std::cout << "  publisher_metrics_csv_path="
            << config.exportConfig().publisherMetricsCsvPath << '\n';
        std::cout << "  metrics_interval_ms="
            << config.exportConfig().metricsIntervalMs << '\n';
    }
}

int main(int argc, char** argv)
{
    const PublisherOptions options = parseOptions(argc, argv);
    const mdp::publisher::config::PublisherConfig config =
        mdp::publisher::config::PublisherConfig::loadFromIni(options.configPath);
    printStartupConfig(options.configPath, config);

    std::unique_ptr<PublisherMetricsCsvWriter> metricsWriter;
    if (config.exportConfig().enablePublisherMetricsCsv)
    {
        metricsWriter = std::make_unique<PublisherMetricsCsvWriter>(
            config.exportConfig().publisherMetricsCsvPath);
    }

    std::unique_ptr<mdp::publisher::IPublisherSource> source =
        mdp::publisher::createPublisherSource(config);
    mdp::publisher::TcpPublisherClientOptions clientOptions;
    clientOptions.processorHost = config.network().processorHost;
    clientOptions.processorPort = config.network().processorPort;
    clientOptions.connectRetryMs = config.network().connectRetryMs;
    clientOptions.maxBatchSize = static_cast<std::uint32_t>(config.runtime().burstSize);
    clientOptions.ackWindowBatches = config.network().ackWindowBatches;
    mdp::publisher::TcpPublisherClient client(clientOptions);
    std::cout << "Connecting to processor at "
        << clientOptions.processorHost << ':' << clientOptions.processorPort
        << "..." << '\n';
    client.connect();

    const std::size_t maxBatchSize = static_cast<std::size_t>(client.maxBatchSize());
    std::cout << "Publisher startup complete. negotiated_max_batch_size="
        << maxBatchSize << '\n';
    std::size_t generatedCount = 0;
    std::size_t encodedCount = 0;
    std::size_t encodeFailureCount = 0;
    std::uint64_t acceptedCount = 0;

    const auto start = std::chrono::steady_clock::now();
    auto previousMetricsTime = start;
    auto nextMetricsTime =
        start + std::chrono::milliseconds(config.exportConfig().metricsIntervalMs);
    PublisherMetricsSample previousMetricsSample;
    const auto stopAt = config.runtime().runtimeSeconds > 0
        ? start + std::chrono::seconds(config.runtime().runtimeSeconds)
        : std::chrono::steady_clock::time_point::max();
    std::vector<mdp::protocol::MarketDataEventFrame> batch;
    batch.reserve(maxBatchSize);

    while (std::chrono::steady_clock::now() < stopAt &&
        (config.runtime().eventCount == 0 ||
            generatedCount < config.runtime().eventCount))
    {
        batch.clear();

        for (std::size_t burstIndex = 0;
            burstIndex < maxBatchSize &&
            std::chrono::steady_clock::now() < stopAt &&
            (config.runtime().eventCount == 0 ||
                generatedCount < config.runtime().eventCount);
            ++burstIndex)
        {
            mdp::MarketDataEvent event;
            if (!source->next(event))
            {
                break;
            }

            ++generatedCount;

            mdp::protocol::MarketDataEventFrame frame;
            if (mdp::protocol::encodeMarketDataEventFrame(event, frame))
            {
                ++encodedCount;
                batch.push_back(frame);
            }
            else
            {
                ++encodeFailureCount;
            }
        }

        if (!batch.empty())
        {
            acceptedCount += client.sendBatch(batch);
        }
        else
        {
            const auto idleWait = source->idleWaitHint();
            if (idleWait.count() > 0 &&
                std::chrono::steady_clock::now() < stopAt)
            {
                std::this_thread::sleep_for(idleWait);
            }
        }

        const auto now = std::chrono::steady_clock::now();
        if (metricsWriter != nullptr &&
            config.exportConfig().metricsIntervalMs > 0 &&
            now >= nextMetricsTime)
        {
            const PublisherMetricsSample currentSample{
                generatedCount,
                encodedCount,
                encodeFailureCount,
                acceptedCount
            };
            const double elapsedSeconds =
                std::chrono::duration_cast<std::chrono::duration<double>>(
                    now - start).count();
            const double intervalSeconds =
                std::chrono::duration_cast<std::chrono::duration<double>>(
                    now - previousMetricsTime).count();

            metricsWriter->write(
                elapsedSeconds,
                intervalSeconds,
                currentSample,
                previousMetricsSample);

            previousMetricsSample = currentSample;
            previousMetricsTime = now;
            nextMetricsTime = now +
                std::chrono::milliseconds(config.exportConfig().metricsIntervalMs);
        }

        if (config.runtime().sleepUs > 0 &&
            std::chrono::steady_clock::now() < stopAt &&
            (config.runtime().eventCount == 0 ||
                generatedCount < config.runtime().eventCount))
        {
            std::this_thread::sleep_for(
                std::chrono::microseconds(config.runtime().sleepUs));
        }
    }

    std::cout << "Publisher runtime elapsed; flushing pending ACKs..." << '\n';
    acceptedCount += client.flushAcks();
    std::cout << "Publisher shutdown complete." << '\n';

    const auto end = std::chrono::steady_clock::now();
    const double elapsedSeconds =
        std::chrono::duration_cast<std::chrono::duration<double>>(end - start).count();

    if (metricsWriter != nullptr)
    {
        const PublisherMetricsSample currentSample{
            generatedCount,
            encodedCount,
            encodeFailureCount,
            acceptedCount
        };
        const double intervalSeconds =
            std::chrono::duration_cast<std::chrono::duration<double>>(
                end - previousMetricsTime).count();

        metricsWriter->write(
            elapsedSeconds,
            intervalSeconds,
            currentSample,
            previousMetricsSample);
        std::cout << "Exported publisher metrics CSV: "
            << config.exportConfig().publisherMetricsCsvPath << '\n';
    }

    std::cout << "Generated events: " << generatedCount << '\n';
    std::cout << "Encoded frames: " << encodedCount << '\n';
    std::cout << "Accepted by processor: " << acceptedCount << '\n';
    std::cout << "Encode failures: " << encodeFailureCount << '\n';
    std::cout << "Elapsed seconds: " << elapsedSeconds << '\n';
    std::cout << "Encode throughput: "
        << perSecond(encodedCount, elapsedSeconds) << " frames/sec\n";

    return encodeFailureCount == 0 ? 0 : 1;
}
