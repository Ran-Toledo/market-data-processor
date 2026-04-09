#pragma once

#include "core/MarketDataEvent.h"

namespace mdp
{
    class IEventRouter
    {
    public:
        virtual ~IEventRouter() = default;

        virtual bool submit(const MarketDataEvent& event) = 0;
    };
}
