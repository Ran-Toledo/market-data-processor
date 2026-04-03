// SyntheticMarketDataSource.cpp
#include "source/SyntheticMarketDataSource.h"
#include "core/AppConfig.h"

#include <chrono>
#include <random>
#include <thread>

namespace
{
    mdp::TimestampNs getCurrentTimestampNs()
    {
        return static_cast<mdp::TimestampNs>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
    }
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
            m_symbols.size() - 1);
        static thread_local std::uniform_real_distribution<double> priceDistribution(100.0, 500.0);
        static thread_local std::uniform_int_distribution<std::uint32_t> volumeDistribution(1, 1000);

        MarketDataEvent event;
        event.symbol = m_symbols[symbolIndexDistribution(generator)];
        event.price = priceDistribution(generator);
        event.volume = volumeDistribution(generator);
        event.exchangeTimestampNs = getCurrentTimestampNs();
        event.ingestTimestampNs = getCurrentTimestampNs();
        event.sequenceNumber = m_nextSequenceNumber++;

        return event;
    }
}
