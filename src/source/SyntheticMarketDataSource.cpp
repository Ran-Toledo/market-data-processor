#include "source/SyntheticMarketDataSource.h"
#include "core/AppConfig.h"
#include "util/Clock.h"

#include <random>
#include <thread>
#include <vector>
#include <string>
#include <cstdio>

namespace mdp::source
{
    namespace
    {
        struct SymbolProfile
        {
            std::string symbol;
            double basePrice;
            double maxDeviation;
        };

        std::vector<SymbolProfile> createSymbolProfiles()
        {
            std::vector<SymbolProfile> profiles;
            profiles.reserve(32);

            // Real symbols
            profiles.push_back({ "AAPL", 185.0, 4.0 });
            profiles.push_back({ "MSFT", 420.0, 6.0 });
            profiles.push_back({ "GOOG", 155.0, 3.0 });
            profiles.push_back({ "AMZN", 180.0, 5.0 });
            profiles.push_back({ "NVDA", 900.0, 20.0 });

            // Synthetic symbols
            for (int i = 0; i < 27; ++i)
            {
                char buffer[16];
                std::snprintf(buffer, sizeof(buffer), "SYM%02d", i);

                const double basePrice = 50.0 + (i * 10.0);
                const double deviation = 2.0 + (i % 5);

                profiles.push_back({ buffer, basePrice, deviation });
            }

            return profiles;
        }

        const std::vector<SymbolProfile> kSymbolProfiles = createSymbolProfiles();
    }

    SyntheticMarketDataSource::SyntheticMarketDataSource()
    {
    }

    bool SyntheticMarketDataSource::next(MarketDataEvent& outEvent)
    {
        if (config::sourceSleepMs > 0)
        {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(config::sourceSleepMs));
        }

        outEvent = generateEvent();
        return true;
    }

    MarketDataEvent SyntheticMarketDataSource::generateEvent()
    {
        static thread_local std::mt19937 generator(std::random_device{}());

        std::uniform_int_distribution<std::size_t> symbolIndexDistribution(
            0,
            kSymbolProfiles.size() - 1);

        static thread_local std::uniform_int_distribution<std::uint32_t> volumeDistribution(1, 1000);

        const SymbolProfile& profile =
            kSymbolProfiles[symbolIndexDistribution(generator)];

        std::uniform_real_distribution<double> priceDistribution(
            profile.basePrice - profile.maxDeviation,
            profile.basePrice + profile.maxDeviation);

        MarketDataEvent event;
        event.symbol = profile.symbol;
        event.price = priceDistribution(generator);
        event.volume = volumeDistribution(generator);

        const TimestampNs timestampNs = clock::nowNs();
        event.exchangeTimestampNs = timestampNs;
        event.ingestTimestampNs = timestampNs;
        event.sequenceNumber = m_nextSequenceNumber++;

        return event;
    }
}
