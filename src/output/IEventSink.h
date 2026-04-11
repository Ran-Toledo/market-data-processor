#pragma once

#include "core/MarketDataEvent.h"
#include "processing/RuleAlert.h"
#include "processing/StateChange.h"

namespace mdp
{
    class IEventSink
    {
    public:
        virtual ~IEventSink() = default;

        virtual void publishProcessedEvent(const MarketDataEvent& event) = 0;
        virtual void publishAlert(const RuleAlert& alert) = 0;
        virtual void publishStateChange(const StateChange& stateChange) = 0;
    };
}
