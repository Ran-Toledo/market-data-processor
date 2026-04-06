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
        std::vector<Symbol> m_symbols;
    };
}
