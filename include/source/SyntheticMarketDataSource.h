// SyntheticMarketDataSource.h
#pragma once

#include "source/IMarketDataSource.h"

#include <cstddef>
#include <random>
#include <unordered_map>
#include <vector>

namespace mdp
{
    class SyntheticMarketDataSource : public IMarketDataSource
    {
    public:
        SyntheticMarketDataSource(
            std::vector<Symbol> symbols,
            std::size_t maxMessages,
            std::uint32_t seed = 42U);

        bool next(MarketDataEvent& outEvent) override;

    private:
        Symbol nextSymbol();
        double nextPrice(const Symbol& symbol);
        std::uint32_t nextVolume();
        TimestampNs nextExchangeTimestamp() const;

    private:
        std::vector<Symbol> m_symbols;
        std::unordered_map<Symbol, double> m_lastPrices;
        std::size_t m_maxMessages{ 0 };
        std::size_t m_generatedMessages{ 0 };
        std::size_t m_symbolIndex{ 0 };
        SequenceNumber m_nextSequenceNumber{ 1 };

        std::mt19937 m_rng;
        std::uniform_int_distribution<std::uint32_t> m_volumeDistribution;
        std::uniform_real_distribution<double> m_priceDeltaDistribution;
    };
}
