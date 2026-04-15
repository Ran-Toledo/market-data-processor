#include "SyntheticPublisherSource.h"
#include "PublisherConfig.h"

#include "api/protocol/MarketDataEventFrame.h"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

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

    std::size_t generatedCount = 0;
    std::size_t encodedCount = 0;
    std::size_t encodeFailureCount = 0;

    const auto start = std::chrono::steady_clock::now();

    for (std::size_t i = 0; i < config.runtime().eventCount;)
    {
        for (std::size_t burstIndex = 0;
            burstIndex < config.runtime().burstSize &&
            i < config.runtime().eventCount;
            ++burstIndex, ++i)
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
            }
            else
            {
                ++encodeFailureCount;
            }
        }

        if (config.runtime().sleepUs > 0 && i < config.runtime().eventCount)
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
    std::cout << "Encode failures: " << encodeFailureCount << '\n';
    std::cout << "Elapsed seconds: " << elapsedSeconds << '\n';
    std::cout << "Encode throughput: "
        << perSecond(encodedCount, elapsedSeconds) << " frames/sec\n";

    return encodeFailureCount == 0 ? 0 : 1;
}
