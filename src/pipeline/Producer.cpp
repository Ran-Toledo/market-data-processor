#include "pipeline/Producer.h"
#include "source/SyntheticMarketDataSource.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace mdp::pipeline
{
    Producer::Producer(IEventRouter& eventRouter, ProducerOptions options)
        : m_sources(createSources(options.producerCount))
        , m_eventRouter(eventRouter)
        , m_options(options)
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
        requestStop();
        join();
    }

    void Producer::requestStop()
    {
        m_running.store(false);
    }

    void Producer::join()
    {
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
        source::IMarketDataSource& source = *m_sources[producerIndex];

        while (m_running.load())
        {
            for (std::size_t i = 0;
                i < m_options.producerBurstSize && m_running.load();
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

                    if (m_options.enableEventLogging)
                    {
                        std::cout << "Produced event | " << event << std::endl;
                    }

                    if (m_options.enableStatsLogging &&
                        m_options.statsLogInterval > 0 &&
                        (producedCount % m_options.statsLogInterval == 0))
                    {
                        std::cout << "Produced events: " << producedCount << std::endl;
                    }
                }
                else
                {
                    const std::size_t rejectedCount =
                        m_rejectedCount.fetch_add(1) + 1;

                    if (m_options.enableEventLogging)
                    {
                        std::cout << "Rejected event | " << rejectedCount << std::endl;
                    }
                }
            }

            if (m_options.producerSleepUs > 0 && m_running.load())
            {
                std::this_thread::sleep_for(
                    std::chrono::microseconds(m_options.producerSleepUs));
            }
        }
    }

    std::vector<std::unique_ptr<source::IMarketDataSource>> Producer::createSources(
        std::size_t producerCount)
    {
        const std::size_t configuredProducerCount =
            std::max<std::size_t>(1, producerCount);
        const std::size_t activeProducerCount = std::min(
            configuredProducerCount,
            source::SyntheticMarketDataSource::getSymbolUniverseSize());

        std::vector<std::unique_ptr<source::IMarketDataSource>> sources;
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
