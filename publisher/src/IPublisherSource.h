#pragma once

#include "api/domain/MarketDataEvent.h"

namespace mdp::publisher
{
    class IPublisherSource
    {
    public:
        virtual ~IPublisherSource() = default;

        virtual bool next(MarketDataEvent& outEvent) = 0;
    };
}
