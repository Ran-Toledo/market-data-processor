// EventProcessor.h
#pragma once

#include "core/MarketDataEvent.h"
#include "processing/SymbolStateStore.h"
#include "processing/SymbolStats.h"

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace mdp
{
    class EventProcessor
    {
    public:
        EventProcessor(
            SymbolStateStore& stateStore,
            SymbolStats& symbolStats);

        void process(const MarketDataEvent& event);

        std::size_t getProcessedCount() const;
        std::uint64_t getTotalLatencyNs() const;
        double getAverageLatencyNs() const;
        std::uint64_t getMinLatencyNs() const;
        std::uint64_t getMaxLatencyNs() const;

    private:
        SymbolStateStore& m_stateStore;
        SymbolStats& m_symbolStats;

        std::atomic<std::size_t> m_processedCount = 0;
        std::atomic<std::uint64_t> m_totalLatencyNs = 0;
        std::atomic<std::uint64_t> m_minLatencyNs = UINT64_MAX;
        std::atomic<std::uint64_t> m_maxLatencyNs = 0;
    };
}
