// IMarketDataSource.h
#pragma once

#include "core/MarketDataEvent.h"

namespace mdp
{
    class IMarketDataSource
    {
    public:
        virtual ~IMarketDataSource() = default;

        virtual bool next(MarketDataEvent& outEvent) = 0;
    };
}
