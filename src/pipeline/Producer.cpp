// Producer.cpp
#include "pipeline/Producer.h"
#include "core/AppConfig.h"

#include <iostream>

using namespace mdp::config;

namespace mdp
{
    Producer::Producer(
        IMarketDataSource& source,
        ThreadSafeQueue<MarketDataEvent>& queue)
        : m_source(source)
        , m_queue(queue)
    {
    }

    Producer::~Producer()
    {
        stop();
    }

    void Producer::start()
    {
        if (m_running.load())
        {
            return;
        }

        m_running = true;
        m_workerThread = std::thread(&Producer::produceLoop, this);
    }

    void Producer::stop()
    {
        if (!m_running.load())
        {
            return;
        }

        m_running = false;

        if (m_workerThread.joinable())
        {
            m_workerThread.join();
        }
    }

    std::size_t Producer::getProducedCount() const
    {
        return m_producedCount.load();
    }

    void Producer::produceLoop()
    {
        while (m_running.load())
        {
            MarketDataEvent event;

            if (!m_source.next(event))
            {
                break;
            }

            if (!m_running.load())
            {
                break;
            }

            m_queue.push(event);

            const std::size_t producedCount = m_producedCount.fetch_add(1) + 1;

            if (enableEventLogging)
            {
                std::cout << "Produced event | " << event << std::endl;
            }

            if (enableProcessingStatsLogging &&
                (producedCount % processingStatsLogInterval == 0))
            {
                std::cout << "Produced events: " << producedCount << std::endl;
            }
        }
    }
}
