#pragma once

namespace mdp
{
    struct MarketDataEvent;
}

namespace mdp::processing
{
    struct RuleAlert;
    struct StateChange;
}

namespace mdp::output
{
    class IEventSink
    {
    public:
        virtual ~IEventSink() = default;

        virtual void publishProcessedEvent(const MarketDataEvent& event) = 0;
        virtual void publishAlert(const processing::RuleAlert& alert) = 0;
        virtual void publishStateChange(const processing::StateChange& stateChange) = 0;
    };
}
