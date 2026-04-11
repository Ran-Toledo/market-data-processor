// SyntheticMarketDataSource.h
#pragma once

#include "source/IMarketDataSource.h"

#include <random>
#include <vector>

namespace mdp::source
{
    class SyntheticMarketDataSource : public IMarketDataSource
    {
    public:
        SyntheticMarketDataSource();
        SyntheticMarketDataSource(std::size_t producerIndex, std::size_t producerCount);

        static std::size_t getSymbolUniverseSize();

        bool next(MarketDataEvent& outEvent) override;

    private:
        MarketDataEvent generateEvent();

    private:
        struct SymbolRuntimeState
        {
            double lastPrice{ 0.0 };
            SequenceNumber nextSequenceNumber{ 0 };
        };

        std::vector<std::size_t> m_symbolIndexes;
        std::vector<SymbolRuntimeState> m_symbolStates;
        std::mt19937 m_generator;
    };
}
