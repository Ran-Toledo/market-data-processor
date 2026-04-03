// SymbolStats.cpp
#include "processing/SymbolStats.h"

namespace mdp
{
    void SymbolStats::record(const MarketDataEvent& event)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

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
        std::lock_guard<std::mutex> lock(m_mutex);

        const auto it = m_symbolStats.find(symbol);
        if (it == m_symbolStats.end())
        {
            return std::nullopt;
        }

        return it->second;
    }

    std::size_t SymbolStats::getTrackedSymbolCount() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_symbolStats.size();
    }
}
