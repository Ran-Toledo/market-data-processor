// IMarketDataSource.h
#pragma once

#include "api/domain/MarketDataEvent.h"

namespace mdp::source
{
    class IMarketDataSource
    {
    public:
        virtual ~IMarketDataSource() = default;

        virtual bool next(MarketDataEvent& outEvent) = 0;
    };
}
