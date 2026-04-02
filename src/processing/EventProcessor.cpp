// EventProcessor.cpp
#include "processing/EventProcessor.h"
#include "core/AppConfig.h"

#include <iostream>

using namespace mdp::config;

namespace mdp
{
    EventProcessor::EventProcessor(ThreadSafeQueue<MarketDataEvent>& queue)
        : m_queue(queue)
    {
    }

    EventProcessor::~EventProcessor()
    {
        stop();
    }

    void EventProcessor::start()
    {
        if (m_running)
        {
            return;
        }

        m_running = true;
        m_workerThread = std::thread(&EventProcessor::processLoop, this);
    }

    void EventProcessor::stop()
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

    std::size_t EventProcessor::getProcessedCount() const
    {
        return m_processedCount.load();
    }

    void EventProcessor::processLoop()
    {
        MarketDataEvent event;

        while (m_queue.pop(event))
        {
            ++m_processedCount;

            if (enableEventLogging)
            {
                std::cout << "Processed event | " << event << std::endl;
            }

            if (enableProcessingStatsLogging &&
                (m_processedCount % processingStatsLogInterval == 0))
            {
                std::cout << "Processed events: " << m_processedCount.load() << std::endl;
            }
        }
    }
}
