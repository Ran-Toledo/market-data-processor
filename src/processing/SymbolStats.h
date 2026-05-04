// SymbolStats.h
#pragma once

#include "api/domain/Types.h"
#include "processing/SymbolStatistics.h"

#include <optional>
#include <unordered_map>

namespace mdp
{
    struct MarketDataEvent;
}

namespace mdp::processing
{
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
