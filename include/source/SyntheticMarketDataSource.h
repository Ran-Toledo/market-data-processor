// SyntheticMarketDataSource.h
#pragma once

#include "source/IMarketDataSource.h"

#include <vector>

namespace mdp
{
    class SyntheticMarketDataSource : public IMarketDataSource
    {
    public:
        SyntheticMarketDataSource();

        bool next(MarketDataEvent& outEvent) override;

    private:
        MarketDataEvent generateEvent();

    private:
        SequenceNumber m_nextSequenceNumber = 1;
        std::vector<Symbol> m_symbols;
    };
}
