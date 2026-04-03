// EventProcessor.cpp
#include "processing/EventProcessor.h"
#include "core/AppConfig.h"

#include <chrono>
#include <iostream>

using namespace mdp::config;

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
        if (m_running.load())
        {
            return;
        }

        m_running = true;
        m_workerThread = std::thread(&EventProcessor::processLoop, this);
    }

    void EventProcessor::stop()
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

    std::size_t EventProcessor::getProcessedCount() const
    {
        return m_processedCount.load();
    }

    std::uint64_t EventProcessor::getTotalLatencyNs() const
    {
        return m_totalLatencyNs.load();
    }

    double EventProcessor::getAverageLatencyNs() const
    {
        const std::size_t processedCount = m_processedCount.load();

        if (processedCount == 0)
        {
            return 0.0;
        }

        return static_cast<double>(m_totalLatencyNs.load()) /
            static_cast<double>(processedCount);
    }

    std::uint64_t EventProcessor::getMinLatencyNs() const
    {
        const std::uint64_t minLatencyNs = m_minLatencyNs.load();
        return minLatencyNs == UINT64_MAX ? 0 : minLatencyNs;
    }

    std::uint64_t EventProcessor::getMaxLatencyNs() const
    {
        return m_maxLatencyNs.load();
    }

    void EventProcessor::processLoop()
    {
        MarketDataEvent event;

        while (m_queue.pop(event))
        {
            const TimestampNs nowNs = getCurrentTimestampNs();
            const std::uint64_t latencyNs =
                nowNs >= event.ingestTimestampNs ? nowNs - event.ingestTimestampNs : 0;

            const std::size_t processedCount = m_processedCount.fetch_add(1) + 1;
            m_totalLatencyNs.fetch_add(latencyNs);

            std::uint64_t currentMin = m_minLatencyNs.load();
            while (latencyNs < currentMin &&
                !m_minLatencyNs.compare_exchange_weak(currentMin, latencyNs))
            {
            }

            std::uint64_t currentMax = m_maxLatencyNs.load();
            while (latencyNs > currentMax &&
                !m_maxLatencyNs.compare_exchange_weak(currentMax, latencyNs))
            {
            }

            if (enableEventLogging)
            {
                std::cout << "Processed event | " << event
                    << " | latency(ns): " << latencyNs << std::endl;
            }

            if (enableProcessingStatsLogging &&
                (processedCount % processingStatsLogInterval == 0))
            {
                std::cout << "Processed events: " << processedCount
                    << " | avg latency(ns): " << getAverageLatencyNs()
                    << " | min latency(ns): " << getMinLatencyNs()
                    << " | max latency(ns): " << getMaxLatencyNs()
                    << std::endl;
            }
        }
    }
}
