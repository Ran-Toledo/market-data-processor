// SymbolStats.h
#pragma once

#include "core/MarketDataEvent.h"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace mdp
{
    struct SymbolStatistics
    {
        std::size_t eventCount = 0;
        std::uint64_t totalVolume = 0;
        double minPrice = 0.0;
        double maxPrice = 0.0;
        double lastPrice = 0.0;
        double averagePrice = 0.0;
    };

    class SymbolStats
    {
    public:
        void record(const MarketDataEvent& event);

        std::optional<SymbolStatistics> tryGet(const Symbol& symbol) const;

        std::unordered_map<Symbol, SymbolStatistics> snapshot() const;

        std::size_t getTrackedSymbolCount() const;

    private:
        mutable std::mutex m_mutex;
        std::unordered_map<Symbol, SymbolStatistics> m_symbolStats;
    };
}
