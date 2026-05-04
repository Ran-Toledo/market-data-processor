#pragma once

#include "api/domain/MarketDataEvent.h"

#include <chrono>

namespace mdp::publisher
{
    class IPublisherSource
    {
    public:
        virtual ~IPublisherSource() = default;

        virtual bool next(MarketDataEvent& outEvent) = 0;
        virtual std::chrono::milliseconds idleWaitHint() const
        {
            return std::chrono::milliseconds(0);
        }
    };
}
