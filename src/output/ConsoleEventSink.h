#pragma once

#include "output/IEventSink.h"

#include <mutex>

namespace mdp
{
    class ConsoleEventSink : public IEventSink
    {
    public:
        void publishProcessedEvent(const MarketDataEvent& event) override;
        void publishAlert(const RuleAlert& alert) override;
        void publishStateChange(const StateChange& stateChange) override;

    private:
        std::mutex m_mutex;
    };
}
