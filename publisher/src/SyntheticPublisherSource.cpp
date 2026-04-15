#include "SyntheticPublisherSource.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

namespace mdp::publisher
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

            profiles.push_back({ "AAPL", 185.0, 4.0 });
            profiles.push_back({ "MSFT", 420.0, 6.0 });
            profiles.push_back({ "GOOG", 155.0, 3.0 });
            profiles.push_back({ "AMZN", 180.0, 5.0 });
            profiles.push_back({ "NVDA", 900.0, 20.0 });

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

        TimestampNs nowNs()
        {
            const auto now = std::chrono::steady_clock::now().time_since_epoch();
            return static_cast<TimestampNs>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
        }

        double clampPrice(double price, const SymbolProfile& profile)
        {
            const double minPrice = profile.basePrice - profile.maxDeviation;
            const double maxPrice = profile.basePrice + profile.maxDeviation;
            return std::clamp(price, minPrice, maxPrice);
        }

        double generateNextPrice(
            double currentPrice,
            const SymbolProfile& profile,
            std::mt19937& generator)
        {
            const double smallMoveStdDev = profile.maxDeviation * 0.08;
            const double spikeMoveStdDev = profile.maxDeviation * 0.35;

            std::normal_distribution<double> smallMoveDistribution(0.0, smallMoveStdDev);
            std::normal_distribution<double> spikeMoveDistribution(0.0, spikeMoveStdDev);
            std::bernoulli_distribution spikeChance(0.03);

            double delta = smallMoveDistribution(generator);
            if (spikeChance(generator))
            {
                delta += spikeMoveDistribution(generator);
            }

            const double meanReversionStrength = 0.015;
            const double meanReversion =
                (profile.basePrice - currentPrice) * meanReversionStrength;

            return clampPrice(currentPrice + delta + meanReversion, profile);
        }

        std::uint32_t generateVolume(std::mt19937& generator)
        {
            std::discrete_distribution<int> bucketDistribution{ 70, 20, 8, 2 };

            switch (bucketDistribution(generator))
            {
            case 0:
                return std::uniform_int_distribution<std::uint32_t>(1, 100)(generator);
            case 1:
                return std::uniform_int_distribution<std::uint32_t>(101, 500)(generator);
            case 2:
                return std::uniform_int_distribution<std::uint32_t>(501, 2000)(generator);
            default:
                return std::uniform_int_distribution<std::uint32_t>(2001, 10000)(generator);
            }
        }

        const std::vector<SymbolProfile> kSymbolProfiles = createSymbolProfiles();
    }

    SyntheticPublisherSource::SyntheticPublisherSource()
        : m_symbolStates(kSymbolProfiles.size())
    {
        std::seed_seq seed
        {
            static_cast<unsigned int>(std::random_device{}()),
            static_cast<unsigned int>(kSymbolProfiles.size())
        };
        m_generator.seed(seed);
    }

    bool SyntheticPublisherSource::next(MarketDataEvent& outEvent)
    {
        outEvent = generateEvent();
        return true;
    }

    MarketDataEvent SyntheticPublisherSource::generateEvent()
    {
        std::uniform_int_distribution<std::size_t> symbolIndexDistribution(
            0,
            kSymbolProfiles.size() - 1);

        const std::size_t symbolIndex = symbolIndexDistribution(m_generator);
        const SymbolProfile& profile = kSymbolProfiles[symbolIndex];
        SymbolRuntimeState& runtimeState = m_symbolStates[symbolIndex];

        if (runtimeState.lastPrice == 0.0)
        {
            runtimeState.lastPrice = profile.basePrice;
        }

        runtimeState.lastPrice =
            generateNextPrice(runtimeState.lastPrice, profile, m_generator);

        MarketDataEvent event;
        event.symbol = profile.symbol;
        event.price = runtimeState.lastPrice;
        event.volume = generateVolume(m_generator);
        event.exchangeTimestampNs = nowNs();
        event.sequenceNumber = ++runtimeState.nextSequenceNumber;

        return event;
    }
}
