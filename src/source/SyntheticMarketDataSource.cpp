// SyntheticMarketDataSource.cpp
#include "source/SyntheticMarketDataSource.h"

#include "core/AppConfig.h"

#include <chrono>
#include <random>
#include <thread>

namespace mdp
{
    namespace
    {
        TimestampNs getCurrentTimestampNs()
        {
            return static_cast<TimestampNs>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count());
        }
    }

    SyntheticMarketDataSource::SyntheticMarketDataSource(ThreadSafeQueue<MarketDataEvent>& queue)
        : m_queue(queue),
        m_symbols{ "AAPL", "MSFT", "GOOG", "AMZN", "NVDA" }
    {
    }

    SyntheticMarketDataSource::~SyntheticMarketDataSource()
    {
        stop();
    }

    void SyntheticMarketDataSource::start()
    {
        if (m_running)
        {
            return;
        }

        m_running = true;
        m_workerThread = std::thread(&SyntheticMarketDataSource::generateLoop, this);
    }

    void SyntheticMarketDataSource::stop()
    {
        if (!m_running)
        {
            return;
        }

        m_running = false;

        if (m_workerThread.joinable())
        {
            m_workerThread.join();
        }
    }

    void SyntheticMarketDataSource::generateLoop()
    {
        while (m_running)
        {
            m_queue.push(generateEvent());
            std::this_thread::sleep_for(std::chrono::milliseconds(config::sourceSleepMs));
        }
    }

    MarketDataEvent SyntheticMarketDataSource::generateEvent()
    {
        static thread_local std::mt19937 rng(std::random_device{}());
        static thread_local std::uniform_int_distribution<std::size_t> symbolIndexDist(0, 4);
        static thread_local std::uniform_real_distribution<double> priceDist(100.0, 500.0);
        static thread_local std::uniform_int_distribution<std::uint32_t> volumeDist(1, 1000);

        MarketDataEvent event;
        event.symbol = m_symbols[symbolIndexDist(rng)];
        event.price = priceDist(rng);
        event.volume = volumeDist(rng);
        event.exchangeTimestampNs = getCurrentTimestampNs();
        event.ingestTimestampNs = getCurrentTimestampNs();
        event.sequenceNumber = m_nextSequenceNumber++;

        return event;
    }
}
