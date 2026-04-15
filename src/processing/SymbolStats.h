// SymbolStats.h
#pragma once

#include "api/domain/MarketDataEvent.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>

namespace mdp::processing
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
        static void mergeInto(
            std::unordered_map<Symbol, SymbolStatistics>& target,
            const Symbol& symbol,
            const SymbolStatistics& sourceStats);

    private:
        std::unordered_map<Symbol, SymbolStatistics> m_symbolStats;
    };
}
