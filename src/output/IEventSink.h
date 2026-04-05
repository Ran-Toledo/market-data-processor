#pragma once

#include "core/MarketDataEvent.h"
#include "processing/RuleAlert.h"

namespace mdp
{
    class IEventSink
    {
    public:
        virtual ~IEventSink() = default;

        virtual void publishProcessedEvent(const MarketDataEvent& event) = 0;
        virtual void publishAlert(const RuleAlert& alert) = 0;
    };
}
