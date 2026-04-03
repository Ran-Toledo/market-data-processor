// EventProcessor.cpp
#include "processing/EventProcessor.h"
#include "core/AppConfig.h"
#include "util/Clock.h"

#include <iostream>

using namespace mdp::config;

namespace mdp
{
    EventProcessor::EventProcessor(
        SymbolStateStore& stateStore,
        SymbolStats& symbolStats)
        : m_stateStore(stateStore)
        , m_symbolStats(symbolStats)
    {
    }

    void EventProcessor::process(const MarketDataEvent& event)
    {
        const TimestampNs nowNs = clock::nowNs();
        const std::uint64_t latencyNs =
            nowNs >= event.ingestTimestampNs ? nowNs - event.ingestTimestampNs : 0;

        m_stateStore.update(event);
        m_symbolStats.record(event);

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
}
