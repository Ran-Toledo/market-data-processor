// SyntheticMarketDataSource.h
#pragma once

#include "source/IMarketDataSource.h"

#include <vector>

namespace mdp::source
{
    class SyntheticMarketDataSource : public IMarketDataSource
    {
    public:
        SyntheticMarketDataSource();

        bool next(MarketDataEvent& outEvent) override;

    private:
        MarketDataEvent generateEvent();

    private:
        struct SymbolRuntimeState
        {
            double lastPrice{ 0.0 };
            SequenceNumber nextSequenceNumber{ 0 };
        };

        std::vector<SymbolRuntimeState> m_symbolStates;
    };
}
