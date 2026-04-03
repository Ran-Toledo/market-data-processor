// SyntheticMarketDataSource.cpp
#include "source/SyntheticMarketDataSource.h"
#include "core/AppConfig.h"
#include "util/Clock.h"

#include <array>
#include <random>
#include <thread>

namespace
{
    struct SymbolProfile
    {
        const char* symbol;
        double basePrice;
        double maxDeviation;
    };

    constexpr std::array<SymbolProfile, 5> kSymbolProfiles =
    { {
        { "AAPL", 185.0, 4.0 },
        { "MSFT", 420.0, 6.0 },
        { "GOOG", 155.0, 3.0 },
        { "AMZN", 180.0, 5.0 },
        { "NVDA", 900.0, 20.0 }
    } };
}

namespace mdp
{
    SyntheticMarketDataSource::SyntheticMarketDataSource()
        : m_symbols{ "AAPL", "MSFT", "GOOG", "AMZN", "NVDA" }
    {
    }

    bool SyntheticMarketDataSource::next(MarketDataEvent& outEvent)
    {
        if (config::sourceSleepMs > 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(config::sourceSleepMs));
        }

        outEvent = generateEvent();
        return true;
    }

    MarketDataEvent SyntheticMarketDataSource::generateEvent()
    {
        static thread_local std::mt19937 generator(std::random_device{}());
        static thread_local std::uniform_int_distribution<std::size_t> symbolIndexDistribution(
            0,
            kSymbolProfiles.size() - 1);
        static thread_local std::uniform_int_distribution<std::uint32_t> volumeDistribution(1, 1000);

        const SymbolProfile& profile = kSymbolProfiles[symbolIndexDistribution(generator)];
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
