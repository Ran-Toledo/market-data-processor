// SymbolStats.cpp
#include "processing/SymbolStats.h"

#include "api/domain/MarketDataEvent.h"

namespace mdp::processing
{
    void SymbolStats::record(const MarketDataEvent& event)
    {
        SymbolStatistics& stats = m_symbolStats[event.symbol];

        if (stats.eventCount == 0)
        {
            stats.minPrice = event.price;
            stats.maxPrice = event.price;
        }
        else
        {
            if (event.price < stats.minPrice)
            {
                stats.minPrice = event.price;
            }

            if (event.price > stats.maxPrice)
            {
                stats.maxPrice = event.price;
            }
        }

        ++stats.eventCount;
        stats.totalVolume += event.volume;
        stats.lastPrice = event.price;
        stats.averagePrice += (event.price - stats.averagePrice) /
            static_cast<double>(stats.eventCount);
    }

    std::optional<SymbolStatistics> SymbolStats::tryGet(const Symbol& symbol) const
    {
        const auto it = m_symbolStats.find(symbol);
        if (it == m_symbolStats.end())
        {
            return std::nullopt;
        }

        return it->second;
    }

    std::unordered_map<Symbol, SymbolStatistics> SymbolStats::snapshot() const
    {
        return m_symbolStats;
    }

    std::size_t SymbolStats::getTrackedSymbolCount() const
    {
        return m_symbolStats.size();
    }

    void SymbolStats::mergeInto(
        std::unordered_map<Symbol, SymbolStatistics>& target,
        const Symbol& symbol,
        const SymbolStatistics& sourceStats)
    {
        if (sourceStats.eventCount == 0)
        {
            return;
        }

        SymbolStatistics& targetStats = target[symbol];
        if (targetStats.eventCount == 0)
        {
            targetStats = sourceStats;
            return;
        }

        const std::size_t mergedEventCount =
            targetStats.eventCount + sourceStats.eventCount;

        targetStats.averagePrice =
            ((targetStats.averagePrice * static_cast<double>(targetStats.eventCount)) +
                (sourceStats.averagePrice * static_cast<double>(sourceStats.eventCount))) /
            static_cast<double>(mergedEventCount);

        targetStats.eventCount = mergedEventCount;
        targetStats.totalVolume += sourceStats.totalVolume;

        if (sourceStats.minPrice < targetStats.minPrice)
        {
            targetStats.minPrice = sourceStats.minPrice;
        }

        if (sourceStats.maxPrice > targetStats.maxPrice)
        {
            targetStats.maxPrice = sourceStats.maxPrice;
        }

        targetStats.lastPrice = sourceStats.lastPrice;
    }
}
