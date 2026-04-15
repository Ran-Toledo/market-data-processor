#pragma once

#include "api/domain/MarketDataEvent.h"

namespace mdp::pipeline
{
    class IEventRouter
    {
    public:
        virtual ~IEventRouter() = default;

        virtual bool submit(const MarketDataEvent& event) = 0;
    };
}
