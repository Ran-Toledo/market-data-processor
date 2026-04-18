#include "SyntheticPublisherSource.h"
#include "PublisherConfig.h"
#include "TcpPublisherClient.h"

#include "api/protocol/MarketDataEventFrame.h"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace
{
    struct PublisherOptions
    {
        std::filesystem::path configPath;
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

    double perSecond(std::size_t count, double elapsedSeconds)
    {
        if (elapsedSeconds <= 0.0)
        {
            return 0.0;
        }

        return static_cast<double>(count) / elapsedSeconds;
    }
}

int main(int argc, char** argv)
{
    const PublisherOptions options = parseOptions(argc, argv);
    const mdp::publisher::config::PublisherConfig config =
        mdp::publisher::config::PublisherConfig::loadFromIni(options.configPath);

    if (config.source().sourceType != "synthetic")
    {
        std::cerr << "Unsupported publisher source type: "
            << config.source().sourceType << '\n';
        return 1;
    }

    mdp::publisher::SyntheticPublisherSource source;
    mdp::publisher::TcpPublisherClientOptions clientOptions;
    clientOptions.processorHost = config.network().processorHost;
    clientOptions.processorPort = config.network().processorPort;
    clientOptions.connectRetryMs = config.network().connectRetryMs;
    clientOptions.maxBatchSize = static_cast<std::uint32_t>(config.runtime().burstSize);
    mdp::publisher::TcpPublisherClient client(clientOptions);
    client.connect();

    const std::size_t maxBatchSize = static_cast<std::size_t>(client.maxBatchSize());
    std::size_t generatedCount = 0;
    std::size_t encodedCount = 0;
    std::size_t encodeFailureCount = 0;
    std::uint64_t acceptedCount = 0;

    const auto start = std::chrono::steady_clock::now();
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
            if (!source.next(event))
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

        if (config.runtime().sleepUs > 0 &&
            std::chrono::steady_clock::now() < stopAt &&
            (config.runtime().eventCount == 0 ||
                generatedCount < config.runtime().eventCount))
        {
            std::this_thread::sleep_for(
                std::chrono::microseconds(config.runtime().sleepUs));
        }
    }

    const auto end = std::chrono::steady_clock::now();
    const double elapsedSeconds =
        std::chrono::duration_cast<std::chrono::duration<double>>(end - start).count();

    std::cout << "Generated events: " << generatedCount << '\n';
    std::cout << "Encoded frames: " << encodedCount << '\n';
    std::cout << "Accepted by processor: " << acceptedCount << '\n';
    std::cout << "Encode failures: " << encodeFailureCount << '\n';
    std::cout << "Elapsed seconds: " << elapsedSeconds << '\n';
    std::cout << "Encode throughput: "
        << perSecond(encodedCount, elapsedSeconds) << " frames/sec\n";

    return encodeFailureCount == 0 ? 0 : 1;
}
