#include "pipeline/Producer.h"
#include "core/AppConfig.h"

#include <chrono>
#include <iostream>
#include <thread>

using namespace mdp::config;

namespace mdp
{
    Producer::Producer(IMarketDataSource& source, WorkerPool& workerPool)
        : m_source(source)
        , m_workerPool(workerPool)
    {
    }

    Producer::~Producer()
    {
        stop();
    }

    void Producer::start()
    {
        if (m_running.exchange(true))
        {
            return;
        }

        m_workerThread = std::thread(&Producer::produceLoop, this);
    }

    void Producer::stop()
    {
        if (!m_running.exchange(false))
        {
            return;
        }

        if (m_workerThread.joinable())
        {
            m_workerThread.join();
        }
    }

    std::size_t Producer::getProducedCount() const
    {
        return m_producedCount.load();
    }

    std::size_t Producer::getRejectedCount() const
    {
        return m_rejectedCount.load();
    }

    std::size_t Producer::getActiveProducerCount() const
    {
        return 1;
    }

    void Producer::produceLoop()
    {
        while (m_running.load())
        {
            for (std::size_t i = 0; i < producerBurstSize && m_running.load(); ++i)
            {
                MarketDataEvent event;

                if (!m_source.next(event))
                {
                    m_running.store(false);
                    break;
                }

                if (!m_running.load())
                {
                    break;
                }

                if (m_workerPool.submit(event))
                {
                    const std::size_t producedCount =
                        m_producedCount.fetch_add(1) + 1;

                    if (enableEventLogging)
                    {
                        std::cout << "Produced event | " << event << std::endl;
                    }

                    if (enableProcessingStatsLogging &&
                        processingStatsLogInterval > 0 &&
                        (producedCount % processingStatsLogInterval == 0))
                    {
                        std::cout << "Produced events: " << producedCount << std::endl;
                    }
                }
                else
                {
                    const std::size_t rejectedCount =
                        m_rejectedCount.fetch_add(1) + 1;

                    if (enableEventLogging)
                    {
                        std::cout << "Rejected event | " << rejectedCount << std::endl;
                    }
                }
            }

            if (producerSleepUs > 0 && m_running.load())
            {
                std::this_thread::sleep_for(std::chrono::microseconds(producerSleepUs));
            }
        }
    }
}
