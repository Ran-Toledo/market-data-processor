#include "pipeline/Producer.h"
#include "core/AppConfig.h"
#include "source/SyntheticMarketDataSource.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace mdp
{
    Producer::Producer(IEventRouter& eventRouter)
        : m_sources(createSources())
        , m_eventRouter(eventRouter)
    {
        if (m_sources.empty())
        {
            throw std::invalid_argument("Producer requires at least one source");
        }
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

        m_workerThreads.reserve(m_sources.size());

        for (std::size_t i = 0; i < m_sources.size(); ++i)
        {
            m_workerThreads.emplace_back(&Producer::produceLoop, this, i);
        }
    }

    void Producer::stop()
    {
        if (!m_running.exchange(false))
        {
            return;
        }

        for (auto& workerThread : m_workerThreads)
        {
            if (workerThread.joinable())
            {
                workerThread.join();
            }
        }

        m_workerThreads.clear();
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
        return m_sources.size();
    }

    void Producer::produceLoop(std::size_t producerIndex)
    {
        IMarketDataSource& source = *m_sources[producerIndex];

        while (m_running.load())
        {
            for (std::size_t i = 0;
                i < config::get().producer().producerBurstSize && m_running.load();
                ++i)
            {
                MarketDataEvent event;

                if (!source.next(event))
                {
                    break;
                }

                if (!m_running.load())
                {
                    break;
                }

                if (m_eventRouter.submit(event))
                {
                    const std::size_t producedCount =
                        m_producedCount.fetch_add(1) + 1;

                    if (config::get().logging().enableEventLogging)
                    {
                        std::cout << "Produced event | " << event << std::endl;
                    }

                    if (config::get().logging().enableProcessingStatsLogging &&
                        config::get().reporting().processingStatsLogInterval > 0 &&
                        (producedCount %
                            config::get().reporting().processingStatsLogInterval == 0))
                    {
                        std::cout << "Produced events: " << producedCount << std::endl;
                    }
                }
                else
                {
                    const std::size_t rejectedCount =
                        m_rejectedCount.fetch_add(1) + 1;

                    if (config::get().logging().enableEventLogging)
                    {
                        std::cout << "Rejected event | " << rejectedCount << std::endl;
                    }
                }
            }

            if (config::get().producer().producerSleepUs > 0 && m_running.load())
            {
                std::this_thread::sleep_for(
                    std::chrono::microseconds(config::get().producer().producerSleepUs));
            }
        }
    }

    std::vector<std::unique_ptr<IMarketDataSource>> Producer::createSources()
    {
        const std::size_t configuredProducerCount =
            std::max<std::size_t>(1, config::get().producer().producerCount);
        const std::size_t activeProducerCount = std::min(
            configuredProducerCount,
            source::SyntheticMarketDataSource::getSymbolUniverseSize());

        std::vector<std::unique_ptr<IMarketDataSource>> sources;
        sources.reserve(activeProducerCount);

        for (std::size_t i = 0; i < activeProducerCount; ++i)
        {
            sources.push_back(
                std::make_unique<source::SyntheticMarketDataSource>(
                    i,
                    activeProducerCount));
        }

        return sources;
    }
}
