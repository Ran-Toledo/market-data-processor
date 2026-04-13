#pragma once

#include "output/IEventSink.h"

#include <mutex>

namespace mdp::output
{
    class ConsoleEventSink : public IEventSink
    {
    public:
        void publishProcessedEvent(const MarketDataEvent& event) override;
        void publishAlert(const processing::RuleAlert& alert) override;
        void publishStateChange(const processing::StateChange& stateChange) override;

    private:
        std::mutex m_mutex;
    };
}
