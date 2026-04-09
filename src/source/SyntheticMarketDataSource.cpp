#include "source/SyntheticMarketDataSource.h"
#include "util/Clock.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

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

            const double nextPrice = currentPrice + delta + meanReversion;
            return clampPrice(nextPrice, profile);
        }

        std::uint32_t generateVolume(std::mt19937& generator)
        {
            std::discrete_distribution<int> bucketDistribution
            {
                70,
                20,
                8,
                2
            };

            switch (bucketDistribution(generator))
            {
            case 0:
            {
                std::uniform_int_distribution<std::uint32_t> distribution(1, 100);
                return distribution(generator);
            }
            case 1:
            {
                std::uniform_int_distribution<std::uint32_t> distribution(101, 500);
                return distribution(generator);
            }
            case 2:
            {
                std::uniform_int_distribution<std::uint32_t> distribution(501, 2000);
                return distribution(generator);
            }
            default:
            {
                std::uniform_int_distribution<std::uint32_t> distribution(2001, 10000);
                return distribution(generator);
            }
            }
        }

        const std::vector<SymbolProfile> kSymbolProfiles = createSymbolProfiles();
    }

    SyntheticMarketDataSource::SyntheticMarketDataSource()
        : SyntheticMarketDataSource(0, 1)
    {
    }

    SyntheticMarketDataSource::SyntheticMarketDataSource(
        std::size_t producerIndex,
        std::size_t producerCount)
        : m_symbolStates(kSymbolProfiles.size())
    {
        if (producerCount == 0)
        {
            producerCount = 1;
        }

        for (std::size_t symbolIndex = producerIndex;
            symbolIndex < kSymbolProfiles.size();
            symbolIndex += producerCount)
        {
            m_symbolIndexes.push_back(symbolIndex);
        }

        std::seed_seq seed
        {
            static_cast<unsigned int>(std::random_device{}()),
            static_cast<unsigned int>(producerIndex),
            static_cast<unsigned int>(producerCount)
        };
        m_generator.seed(seed);
    }

    std::size_t SyntheticMarketDataSource::getSymbolUniverseSize()
    {
        return kSymbolProfiles.size();
    }

    bool SyntheticMarketDataSource::next(MarketDataEvent& outEvent)
    {
        if (m_symbolIndexes.empty())
        {
            return false;
        }

        outEvent = generateEvent();
        return true;
    }

    MarketDataEvent SyntheticMarketDataSource::generateEvent()
    {
        std::uniform_int_distribution<std::size_t> symbolIndexDistribution(
            0,
            m_symbolIndexes.size() - 1);

        const std::size_t symbolIndex = m_symbolIndexes[symbolIndexDistribution(m_generator)];
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

        const TimestampNs timestampNs = clock::nowNs();
        event.exchangeTimestampNs = timestampNs;
        event.ingestTimestampNs = timestampNs;
        event.sequenceNumber = ++runtimeState.nextSequenceNumber;

        return event;
    }
}
